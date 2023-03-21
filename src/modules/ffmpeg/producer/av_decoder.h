#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
extern "C" {
#include <libavutil/avutil.h>
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <memory>
#include <string>
#include <atomic>
#include <queue>

#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

// TODO (fix) Handle ts discontinuities.
// TODO (feat) Forward options.
namespace caspar { namespace ffmpeg {
class Decoder
{
    Decoder(const Decoder&)            = delete;
    Decoder& operator=(const Decoder&) = delete;

    AVStream*         st       = nullptr;
    int64_t           next_pts = AV_NOPTS_VALUE;
    std::atomic<bool> eof      = {false};

    std::queue<std::shared_ptr<AVPacket>> input;
    mutable boost::mutex                  input_mutex;
    boost::condition_variable             input_cond;
    int                                   input_capacity = 2;

    std::queue<std::shared_ptr<AVFrame>> output;
    mutable boost::mutex                 output_mutex;
    boost::condition_variable            output_cond;
    int                                  output_capacity = 8;

    boost::thread thread;

  public:
    std::shared_ptr<AVCodecContext> ctx;

    Decoder() = default;

    explicit Decoder(AVStream* stream);
    ~Decoder();
    bool                     want_packet() const;
    void                     push(std::shared_ptr<AVPacket> packet);
    std::shared_ptr<AVFrame> pop();
};
}} // namespace caspar::ffmpeg
