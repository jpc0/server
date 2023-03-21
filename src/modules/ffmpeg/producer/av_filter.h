#pragma once

#include "av_input.h"
#include "av_decoder.h"

#include "../util/av_assert.h"
#include "../util/av_util.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
extern "C" {
#include <libavfilter/avfilter.h>
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <memory>
#include <map>
#include <string>


namespace caspar { namespace ffmpeg {
struct Filter
{
    std::shared_ptr<AVFilterGraph>  graph;
    AVFilterContext*                sink = nullptr;
    std::map<int, AVFilterContext*> sources;
    std::shared_ptr<AVFrame>        frame;
    bool                            eof = false;

    Filter() = default;

    Filter(std::string                    filter_spec,
           const Input&                   input,
           std::map<int, Decoder>&        streams,
           int64_t                        start_time,
           AVMediaType                    media_type,
           const core::video_format_desc& format_desc);

    bool operator()(int nb_samples = -1);
};
}} // namespace caspar::ffmpeg
