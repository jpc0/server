#include "av_decoder.h"

#include "../util/av_assert.h"
#include "../util/av_util.h"

#include <boost/property_tree/ptree.hpp>

#include <common/env.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace caspar { namespace ffmpeg {
Decoder::Decoder(AVStream* stream)
        : st(stream)
    {
        const auto codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!codec) {
            FF_RET(AVERROR_DECODER_NOT_FOUND, "avcodec_find_decoder");
        }

        ctx = std::shared_ptr<AVCodecContext>(avcodec_alloc_context3(codec),
                                              [](AVCodecContext* ptr) { avcodec_free_context(&ptr); });

        if (!ctx) {
            FF_RET(AVERROR(ENOMEM), "avcodec_alloc_context3");
        }

        FF(avcodec_parameters_to_context(ctx.get(), stream->codecpar));

        int thread_count = env::properties().get(L"configuration.ffmpeg.producer.threads", 0);
        FF(av_opt_set_int(ctx.get(), "threads", thread_count, 0));

        ctx->pkt_timebase = stream->time_base;

        if (ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            ctx->framerate           = av_guess_frame_rate(nullptr, stream, nullptr);
            ctx->sample_aspect_ratio = av_guess_sample_aspect_ratio(nullptr, stream, nullptr);
        } else if (ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (!ctx->channel_layout && ctx->channels) {
                ctx->channel_layout = av_get_default_channel_layout(ctx->channels);
            }
            if (!ctx->channels && ctx->channel_layout) {
                ctx->channels = av_get_channel_layout_nb_channels(ctx->channel_layout);
            }
        }

        if (codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
            ctx->thread_type = FF_THREAD_SLICE;
        }

        FF(avcodec_open2(ctx.get(), codec, nullptr));

        thread = boost::thread([=]() {
            try {
                while (!thread.interruption_requested()) {
                    auto av_frame = alloc_frame();
                    auto ret      = avcodec_receive_frame(ctx.get(), av_frame.get());

                    if (ret == AVERROR(EAGAIN)) {
                        std::shared_ptr<AVPacket> packet;
                        {
                            boost::unique_lock<boost::mutex> lock(input_mutex);
                            input_cond.wait(lock, [&]() { return !input.empty(); });
                            packet = std::move(input.front());
                            input.pop();
                        }
                        FF(avcodec_send_packet(ctx.get(), packet.get()));
                    } else if (ret == AVERROR_EOF) {
                        avcodec_flush_buffers(ctx.get());
                        av_frame->pts = next_pts;
                        next_pts      = AV_NOPTS_VALUE;
                        eof           = true;

                        {
                            boost::unique_lock<boost::mutex> lock(output_mutex);
                            output_cond.wait(lock, [&]() { return output.size() < output_capacity; });
                            output.push(std::move(av_frame));
                        }
                    } else {
                        FF_RET(ret, "avcodec_receive_frame");

                        // NOTE This is a workaround for DVCPRO HD.
                        if (av_frame->width > 1024 && av_frame->interlaced_frame) {
                            av_frame->top_field_first = 1;
                        }

                        // TODO (fix) is this always best?
                        av_frame->pts = av_frame->best_effort_timestamp;

                        auto duration_pts = av_frame->pkt_duration;
                        if (duration_pts <= 0) {
                            if (ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                                const auto ticks = av_stream_get_parser(st) ? av_stream_get_parser(st)->repeat_pict + 1
                                                                            : ctx->ticks_per_frame;
                                duration_pts     = static_cast<int64_t>(AV_TIME_BASE) * ctx->framerate.den * ticks /
                                               ctx->framerate.num / ctx->ticks_per_frame;
                                duration_pts = av_rescale_q(duration_pts, {1, AV_TIME_BASE}, st->time_base);
                            } else if (ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
                                duration_pts = av_rescale_q(av_frame->nb_samples, {1, ctx->sample_rate}, st->time_base);
                            }
                        }

                        if (duration_pts > 0) {
                            next_pts = av_frame->pts + duration_pts;
                        } else {
                            next_pts = AV_NOPTS_VALUE;
                        }

                        {
                            boost::unique_lock<boost::mutex> lock(output_mutex);
                            output_cond.wait(lock, [&]() { return output.size() < output_capacity; });
                            output.push(std::move(av_frame));
                        }
                    }
                }
            } catch (boost::thread_interrupted&) {
                // Do nothing...
            }
        });
    }

 Decoder::~Decoder()
    {
        try {
            if (thread.joinable()) {
                thread.interrupt();
                thread.join();
            }
        } catch (boost::thread_interrupted&) {
            // Do nothing...
        }
    }

bool Decoder::want_packet() const
    {
        if (eof) {
            return false;
        }

        {
            boost::lock_guard<boost::mutex> lock(input_mutex);
            return input.size() < input_capacity;
        }
    }

void Decoder::push(std::shared_ptr<AVPacket> packet)
    {
        if (eof) {
            return;
        }

        {
            boost::lock_guard<boost::mutex> lock(input_mutex);
            input.push(std::move(packet));
        }

        input_cond.notify_all();
    }

std::shared_ptr<AVFrame> Decoder::pop()
    {
        std::shared_ptr<AVFrame> frame;

        {
            boost::lock_guard<boost::mutex> lock(output_mutex);

            if (!output.empty()) {
                frame = std::move(output.front());
                output.pop();
            }
        }

        if (frame) {
            output_cond.notify_all();
        } else if (eof) {
            frame = alloc_frame();
        }

        return frame;
    }
}} // namespace caspar::ffmpeg
