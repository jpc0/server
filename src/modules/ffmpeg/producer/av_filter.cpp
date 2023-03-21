#include "av_filter.h"
#include "av_common.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/format.hpp>

#include <common/env.h>
#include <common/scope_exit.h>


namespace caspar { namespace ffmpeg {
Filter::Filter(std::string                    filter_spec,
               const Input&                   input,
               std::map<int, Decoder>&        streams,
               int64_t                        start_time,
               AVMediaType                    media_type,
               const core::video_format_desc& format_desc)
{
    if (media_type == AVMEDIA_TYPE_VIDEO) {
        if (filter_spec.empty()) {
            filter_spec = "null";
        }

        auto deint =
            u8(env::properties().get<std::wstring>(L"configuration.ffmpeg.producer.auto-deinterlace", L"interlaced"));

        if (deint != "none") {
            filter_spec += (boost::format(",bwdif=mode=send_field:parity=auto:deint=%s") % deint).str();
        }

        filter_spec += (boost::format(",fps=fps=%d/%d:start_time=%f") %
                        (format_desc.framerate.numerator() * format_desc.field_count) %
                        format_desc.framerate.denominator() % (static_cast<double>(start_time) / AV_TIME_BASE))
                           .str();
    } else if (media_type == AVMEDIA_TYPE_AUDIO) {
        if (filter_spec.empty()) {
            filter_spec = "anull";
        }

        // Find first audio stream to get a time_base for the first_pts calculation
        AVRational tb = {1, format_desc.audio_sample_rate};
        for (auto n = 0U; n < input->nb_streams; ++n) {
            const auto st = input->streams[n];
            if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && st->codecpar->channels > 0) {
                tb = {1, st->codecpar->sample_rate};
                break;
            }
        }
        filter_spec += (boost::format(",aresample=async=1000:first_pts=%d:min_comp=0.01:osr=%d,"
                                      "asetnsamples=n=1024:p=0") %
                        av_rescale_q(start_time, TIME_BASE_Q, tb) % format_desc.audio_sample_rate)
                           .str();
    }

    AVFilterInOut* outputs = nullptr;
    AVFilterInOut* inputs  = nullptr;

    CASPAR_SCOPE_EXIT
    {
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
    };

    int video_input_count = 0;
    int audio_input_count = 0;
    {
        auto graph2 = avfilter_graph_alloc();
        if (!graph2) {
            FF_RET(AVERROR(ENOMEM), "avfilter_graph_alloc");
        }

        CASPAR_SCOPE_EXIT
        {
            avfilter_graph_free(&graph2);
            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);
        };

        FF(avfilter_graph_parse2(graph2, filter_spec.c_str(), &inputs, &outputs));

        for (auto cur = inputs; cur; cur = cur->next) {
            const auto type = avfilter_pad_get_type(cur->filter_ctx->input_pads, cur->pad_idx);
            if (type == AVMEDIA_TYPE_VIDEO) {
                video_input_count += 1;
            } else if (type == AVMEDIA_TYPE_AUDIO) {
                audio_input_count += 1;
            }
        }
    }

    std::vector<AVStream*> av_streams;
    for (auto n = 0U; n < input->nb_streams; ++n) {
        const auto st = input->streams[n];

        if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && st->codecpar->channels == 0) {
            continue;
        }

        auto disposition = st->disposition;
        if (!disposition || disposition == AV_DISPOSITION_DEFAULT) {
            av_streams.push_back(st);
        }
    }

    if (audio_input_count == 1) {
        auto count = std::count_if(
            av_streams.begin(), av_streams.end(), [](auto s) { return s->codecpar->codec_type == AVMEDIA_TYPE_AUDIO; });

        // TODO (fix) Use some form of stream meta data to do this.
        // https://github.com/CasparCG/server/issues/833
        if (count > 1) {
            filter_spec = (boost::format("amerge=inputs=%d,") % count).str() + filter_spec;
        }
    }

    if (video_input_count == 1) {
        std::stable_sort(av_streams.begin(), av_streams.end(), [](auto lhs, auto rhs) {
            return lhs->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && lhs->codecpar->height > rhs->codecpar->height;
        });

        std::vector<AVStream*> video_av_streams;
        std::copy_if(av_streams.begin(), av_streams.end(), std::back_inserter(video_av_streams), [](auto s) {
            return s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO;
        });

        // TODO (fix) Use some form of stream meta data to do this.
        // https://github.com/CasparCG/server/issues/832
        if (video_av_streams.size() >= 2 &&
            video_av_streams[0]->codecpar->height == video_av_streams[1]->codecpar->height) {
            filter_spec = "alphamerge," + filter_spec;
        }
    }

    graph =
        std::shared_ptr<AVFilterGraph>(avfilter_graph_alloc(), [](AVFilterGraph* ptr) { avfilter_graph_free(&ptr); });

    if (!graph) {
        FF_RET(AVERROR(ENOMEM), "avfilter_graph_alloc");
    }

    FF(avfilter_graph_parse2(graph.get(), filter_spec.c_str(), &inputs, &outputs));

    // inputs
    {
        for (auto cur = inputs; cur; cur = cur->next) {
            const auto type = avfilter_pad_get_type(cur->filter_ctx->input_pads, cur->pad_idx);
            if (type != AVMEDIA_TYPE_VIDEO && type != AVMEDIA_TYPE_AUDIO) {
                CASPAR_THROW_EXCEPTION(ffmpeg_error_t() << boost::errinfo_errno(EINVAL)
                                                        << msg_info_t("only video and audio filters supported"));
            }

            unsigned index = 0;

            // TODO find stream based on link name
            while (true) {
                if (index == av_streams.size()) {
                    graph = nullptr;
                    return;
                }
                if (av_streams.at(index)->codecpar->codec_type == type &&
                    sources.find(static_cast<int>(index)) == sources.end()) {
                    break;
                }
                index++;
            }

            index = av_streams.at(index)->index;

            auto it = streams.find(index);
            if (it == streams.end()) {
                it = streams.emplace(index, input->streams[index]).first;
            }

            auto& st = it->second.ctx;

            if (st->codec_type == AVMEDIA_TYPE_VIDEO) {
                auto args = (boost::format("video_size=%dx%d:pix_fmt=%d:time_base=%d/%d") % st->width % st->height %
                             st->pix_fmt % st->pkt_timebase.num % st->pkt_timebase.den)
                                .str();
                auto name = (boost::format("in_%d") % index).str();

                if (st->sample_aspect_ratio.num > 0 && st->sample_aspect_ratio.den > 0) {
                    args +=
                        (boost::format(":sar=%d/%d") % st->sample_aspect_ratio.num % st->sample_aspect_ratio.den).str();
                }

                if (st->framerate.num > 0 && st->framerate.den > 0) {
                    args += (boost::format(":frame_rate=%d/%d") % st->framerate.num % st->framerate.den).str();
                }

                AVFilterContext* source = nullptr;
                FF(avfilter_graph_create_filter(
                    &source, avfilter_get_by_name("buffer"), name.c_str(), args.c_str(), nullptr, graph.get()));
                FF(avfilter_link(source, 0, cur->filter_ctx, cur->pad_idx));
                sources.emplace(index, source);
            } else if (st->codec_type == AVMEDIA_TYPE_AUDIO) {
                auto args = (boost::format("time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%#x") %
                             st->pkt_timebase.num % st->pkt_timebase.den % st->sample_rate %
                             av_get_sample_fmt_name(st->sample_fmt) % st->channel_layout)
                                .str();
                auto name = (boost::format("in_%d") % index).str();

                AVFilterContext* source = nullptr;
                FF(avfilter_graph_create_filter(
                    &source, avfilter_get_by_name("abuffer"), name.c_str(), args.c_str(), nullptr, graph.get()));
                FF(avfilter_link(source, 0, cur->filter_ctx, cur->pad_idx));
                sources.emplace(index, source);
            } else {
                CASPAR_THROW_EXCEPTION(ffmpeg_error_t() << boost::errinfo_errno(EINVAL)
                                                        << msg_info_t("invalid filter input media type"));
            }
        }
    }

    if (media_type == AVMEDIA_TYPE_VIDEO) {
        FF(avfilter_graph_create_filter(
            &sink, avfilter_get_by_name("buffersink"), "out", nullptr, nullptr, graph.get()));

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4245)
#endif
        const AVPixelFormat pix_fmts[] = {AV_PIX_FMT_RGB24,
                                          AV_PIX_FMT_BGR24,
                                          AV_PIX_FMT_BGRA,
                                          AV_PIX_FMT_ARGB,
                                          AV_PIX_FMT_RGBA,
                                          AV_PIX_FMT_ABGR,
                                          AV_PIX_FMT_YUV444P,
                                          AV_PIX_FMT_YUV422P,
                                          AV_PIX_FMT_YUV420P,
                                          AV_PIX_FMT_YUV410P,
                                          AV_PIX_FMT_YUVA444P,
                                          AV_PIX_FMT_YUVA422P,
                                          AV_PIX_FMT_YUVA420P,
                                          AV_PIX_FMT_UYVY422,
                                          AV_PIX_FMT_NONE};
        FF(av_opt_set_int_list(sink, "pix_fmts", pix_fmts, -1, AV_OPT_SEARCH_CHILDREN));
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    } else if (media_type == AVMEDIA_TYPE_AUDIO) {
        FF(avfilter_graph_create_filter(
            &sink, avfilter_get_by_name("abuffersink"), "out", nullptr, nullptr, graph.get()));
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4245)
#endif
        const AVSampleFormat sample_fmts[] = {AV_SAMPLE_FMT_S32, AV_SAMPLE_FMT_NONE};
        FF(av_opt_set_int_list(sink, "sample_fmts", sample_fmts, -1, AV_OPT_SEARCH_CHILDREN));

        // TODO Always output 8 channels and remove hack in make_frame.

        const int sample_rates[] = {format_desc.audio_sample_rate, -1};
        FF(av_opt_set_int_list(sink, "sample_rates", sample_rates, -1, AV_OPT_SEARCH_CHILDREN));
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    } else {
        CASPAR_THROW_EXCEPTION(ffmpeg_error_t()
                               << boost::errinfo_errno(EINVAL) << msg_info_t("invalid output media type"));
    }

    // output
    {
        const auto cur = outputs;

        if (!cur || cur->next) {
            CASPAR_THROW_EXCEPTION(ffmpeg_error_t()
                                   << boost::errinfo_errno(EINVAL) << msg_info_t("invalid filter graph output count"));
        }

        if (avfilter_pad_get_type(cur->filter_ctx->output_pads, cur->pad_idx) != media_type) {
            CASPAR_THROW_EXCEPTION(ffmpeg_error_t()
                                   << boost::errinfo_errno(EINVAL) << msg_info_t("invalid filter output media type"));
        }

        FF(avfilter_link(cur->filter_ctx, cur->pad_idx, sink, 0));
    }

    FF(avfilter_graph_config(graph.get(), nullptr));

    CASPAR_LOG(debug) << avfilter_graph_dump(graph.get(), nullptr);
}

bool Filter::operator()(int nb_samples)
{
    if (frame || eof) {
        return false;
    }

    if (!sink || sources.empty()) {
        eof   = true;
        frame = nullptr;
        return true;
    }

    auto av_frame = alloc_frame();
    auto ret      = nb_samples >= 0 ? av_buffersink_get_samples(sink, av_frame.get(), nb_samples)
                                    : av_buffersink_get_frame(sink, av_frame.get());

    if (ret == AVERROR(EAGAIN)) {
        return false;
    }
    if (ret == AVERROR_EOF) {
        eof   = true;
        frame = nullptr;
        return true;
    }
    FF_RET(ret, "av_buffersink_get_frame");
    frame = std::move(av_frame);
    return true;
}
}} // namespace caspar::ffmpeg
