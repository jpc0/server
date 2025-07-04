/*
 * Copyright (c) 2011 Sveriges Television AB <info@casparcg.com>
 *
 * This file is part of CasparCG (www.casparcg.com).
 *
 * CasparCG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CasparCG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Robert Nagy, ronag89@gmail.com
 */

#include "../StdAfx.h"
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/Rect.hpp>

#include "osd_graph.h"

#pragma warning(disable : 4244)

#include "call_context.h"

#include <common/executor.h>
#include <common/timer.h>

#include <SFML/Graphics.hpp>

#include <boost/circular_buffer.hpp>

#include <tbb/concurrent_unordered_map.h>

#include <GL/glew.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace fs = std::filesystem;

namespace caspar { namespace core { namespace diagnostics { namespace osd {

static const int PREFERRED_VERTICAL_GRAPHS = 8;
static const int RENDERING_WIDTH           = 1024;
static const int RENDERING_HEIGHT          = RENDERING_WIDTH / PREFERRED_VERTICAL_GRAPHS;

sf::Color get_sfml_color(int color)
{
    auto c = caspar::diagnostics::color(color);

    return {static_cast<std::uint8_t>(color >> 24 & 255),
            static_cast<std::uint8_t>(color >> 16 & 255),
            static_cast<std::uint8_t>(color >> 8 & 255),
            static_cast<std::uint8_t>(color >> 0 & 255)};
}

auto& get_default_font()
{
    static sf::Font DEFAULT_FONT = []() {
        fs::path path{DIAG_FONT_PATH};
#ifdef __linux__
        if (!fs::exists(path)) {
            auto cmd = "fc-match --format=%{file} " + path.string();
            if (auto pipe = popen(cmd.data(), "r")) {
                char buf[128];
                path.clear();
                while (fgets(buf, sizeof(buf), pipe))
                    path += buf;
            }
        }
#endif
        sf::Font font;
        if (!font.openFromFile(path.string()))
            CASPAR_THROW_EXCEPTION(file_not_found() << msg_info(DIAG_FONT_PATH " not found"));
        return font;
    }();

    return DEFAULT_FONT;
}

struct drawable
    : public sf::Drawable
    , public sf::Transformable
{
    virtual ~drawable() {}
    virtual void render(sf::RenderTarget& target, sf::RenderStates states) = 0;

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override
    {
        states.transform *= getTransform();
        const_cast<drawable*>(this)->render(target, states);
    }
};

class context : public drawable
{
    std::unique_ptr<sf::RenderWindow> window_;
    sf::View                          view_;

    std::list<std::weak_ptr<drawable>> drawables_;
    caspar::timer                      display_time_;
    bool                               calculate_view_  = true;
    int                                scroll_position_ = 0;
    bool                               dragging_        = false;
    int                                last_mouse_y_    = 0;

    executor executor_{L"diagnostics"};

  public:
    static void register_drawable(const std::shared_ptr<drawable>& drawable)
    {
        if (!drawable)
            return;

        get_instance()->executor_.begin_invoke([=] { get_instance()->do_register_drawable(drawable); });
    }

    static void show(bool value)
    {
        get_instance()->executor_.begin_invoke([=] { get_instance()->do_show(value); });
    }

    static void shutdown() { get_instance().reset(); }

  private:
    context() {}

    void do_show(bool value)
    {
        sf::VideoMode s{};
        if (value) {
            if (!window_) {
                window_.reset(
                    new sf::RenderWindow(sf::VideoMode({RENDERING_WIDTH, RENDERING_WIDTH}), "CasparCG Diagnostics"));
                window_->setPosition(sf::Vector2i(0, 0));
                [[maybe_unused]] auto active = window_->setActive();
                window_->setVerticalSyncEnabled(true);
                calculate_view_ = true;
                glEnable(GL_BLEND);
                glEnable(GL_LINE_SMOOTH);
                glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                tick();
            }
        } else
            window_.reset();
    }

    void tick()
    {
        if (!window_)
            return;

        while (auto opt_event = window_->pollEvent()) {
            if (!opt_event.has_value()) {
                break;
            }
            sf::Event& e = opt_event.value();
            e.visit([&](auto&& event) {
                using T = std::remove_reference<decltype(event)>;
                if constexpr (std::is_same_v<sf::Event::Closed, T>) {
                    window_.reset();
                    return;
                }
                if constexpr (std::is_same_v<sf::Event::Resized, T>) {
                    calculate_view_ = true;
                } else if constexpr (std::is_same_v<sf::Event::MouseButtonPressed, T>) {
                    dragging_     = true;
                    last_mouse_y_ = event.mouseButton.y;
                } else if constexpr (std::is_same_v<sf::Event::MouseButtonReleased, T>) {
                    dragging_ = false;
                } else if constexpr (std::is_same_v<sf::Event::MouseMoved, T>) {
                    if (dragging_) {
                        auto delta_y = event.mouseMove.y - last_mouse_y_;
                        scroll_position_ += delta_y;
                        last_mouse_y_   = event.mouseMove.y;
                        calculate_view_ = true;
                    }
                } else if constexpr (std::is_same_v<sf::Event::MouseWheelScrolled, T>) {
                    scroll_position_ += event.mouseWheel.delta * 15;
                    calculate_view_ = true;
                }
            });
        }

        window_->clear();

        if (calculate_view_) {
            auto content_height      = static_cast<float>(RENDERING_HEIGHT * drawables_.size());
            auto window_height       = static_cast<float>(window_->getSize().y);
            int  not_visible         = std::max(0.0F, content_height - window_height);
            int  min_scroll_position = -not_visible;
            int  max_scroll_position = 0;

            scroll_position_ = std::min(max_scroll_position, std::max(min_scroll_position, scroll_position_));
            sf::FloatRect a;
            view_.setViewport(sf::FloatRect({0, 0}, {1.0, 1.0}));
            view_.setSize({RENDERING_WIDTH, window_height});
            view_.setCenter({RENDERING_WIDTH / 2.0F, window_height / 2.0F - scroll_position_});
            window_->setView(view_);

            calculate_view_ = false;
        }

        CASPAR_LOG(trace) << "osd_graph::tick()";
        window_->draw(*this);
        window_->display();

        display_time_.restart();
        if (executor_.is_running()) {
            executor_.begin_invoke([this] { tick(); });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    void render(sf::RenderTarget& target, sf::RenderStates states) override
    {
        int n = 0;

        for (auto it = drawables_.begin(); it != drawables_.end(); ++n) {
            auto drawable = it->lock();
            if (drawable) {
                float target_y = n * RENDERING_HEIGHT;
                drawable->setPosition({0.0f, target_y});
                target.draw(*drawable, states);
                ++it;
            } else
                it = drawables_.erase(it);
        }
    }

    void do_register_drawable(const std::shared_ptr<drawable>& drawable)
    {
        drawables_.push_back(drawable);
        auto it = drawables_.begin();
        while (it != drawables_.end()) {
            if (it->lock())
                ++it;
            else
                it = drawables_.erase(it);
        }
    }

    static std::unique_ptr<context>& get_instance()
    {
        static auto impl = std::unique_ptr<context>(new context);
        return impl;
    }
};

class line : public drawable
{
    size_t                                                 res_{1024};
    boost::circular_buffer<sf::Vertex>                     line_data_{res_};
    boost::circular_buffer<std::optional<sf::VertexArray>> line_tags_{res_};

    std::atomic<float> tick_data_;
    std::atomic<bool>  tick_tag_;
    std::atomic<int>   color_;

    double x_delta_ = 1.0 / (static_cast<double>(res_) - 1.0);

  public:
    line()
        : tick_data_(-1.0f)
        , tick_tag_(false)
        , color_(0xFFFFFFFF)
    {
    }

    line(const line& other)
        : res_(other.res_)
        , line_data_(other.line_data_)
        , line_tags_(other.line_tags_)
        , tick_data_(other.tick_data_.load())
        , tick_tag_(other.tick_tag_.load())
        , color_(other.color_.load())
        , x_delta_(other.x_delta_)
    {
    }

    void set_value(float value) { tick_data_ = value; }

    void set_tag() { tick_tag_ = true; }

    void set_color(int color) { color_ = color; }

    int get_color() { return color_; }

    void render(sf::RenderTarget& target, sf::RenderStates states) override
    {
        /*states.transform.translate(x_pos_, 0.f);

        if (line_data_.size() == res_)
            x_pos_ = -get_insertion_xcoord() + 1.0 - x_delta_; // Otherwise the graph will drift because of floating
        point precision else x_pos_ -= x_delta_;*/

        for (auto& vertex : line_data_)
            vertex.position.x -= x_delta_;

        for (auto& tag : line_tags_) {
            if (tag) {
                (*tag)[0].position.x -= x_delta_;
                (*tag)[1].position.x -= x_delta_;
            }
        }

        auto color = get_sfml_color(color_);
        color.a    = 255 * 0.8;
        sf::Vertex a;
        line_data_.push_back(sf::Vertex{sf::Vector2f{static_cast<float>(get_insertion_xcoord()),
                                                     std::max(0.1f, std::min(0.9f, (1.0f - tick_data_) * 0.8f + 0.1f))},
                                        color});

        if (tick_tag_) {
            sf::VertexArray vertical_dash(sf::PrimitiveType::LineStrip);
            vertical_dash.append(
                sf::Vertex{sf::Vector2f{static_cast<float>(get_insertion_xcoord() - x_delta_), 0.f}, color});
            vertical_dash.append(
                sf::Vertex{sf::Vector2f{static_cast<float>(get_insertion_xcoord() - x_delta_), 1.f}, color});
            line_tags_.push_back(vertical_dash);
        } else
            line_tags_.push_back({});

        tick_tag_ = false;

        if (tick_data_ > -0.5) {
            auto array_one = line_data_.array_one();
            auto array_two = line_data_.array_two();
            // since boost::circular_buffer guarantees two contiguous views of the buffer we can provide raw access to
            // SFML, which can use glDrawArrays.
            target.draw(
                array_one.first, static_cast<unsigned int>(array_one.second), sf::PrimitiveType::LineStrip, states);
            target.draw(
                array_two.first, static_cast<unsigned int>(array_two.second), sf::PrimitiveType::LineStrip, states);

            if (array_one.second > 0 && array_two.second > 0) {
                // Connect the gap between the arrays
                sf::VertexArray connecting_line(sf::PrimitiveType::LineStrip);
                connecting_line.append(*(array_one.first + array_one.second - 1));
                connecting_line.append(*array_two.first);
                target.draw(connecting_line, states);
            }
        } else {
            glEnable(GL_LINE_STIPPLE);
            glLineStipple(3, 0xAAAA);

            for (size_t n = 0; n < line_tags_.size(); ++n) {
                if (line_tags_[n]) {
                    target.draw(*line_tags_[n], states);
                }
            }

            glDisable(GL_LINE_STIPPLE);
        }
    }

  private:
    double get_insertion_xcoord() const { return line_data_.empty() ? 1.0 : line_data_.back().position.x + x_delta_; }
};

struct graph
    : public drawable
    , public caspar::diagnostics::spi::graph_sink
    , public std::enable_shared_from_this<graph>
{
    call_context                                     context_ = call_context::for_thread();
    tbb::concurrent_unordered_map<std::string, line> lines_;

    std::mutex   mutex_;
    std::wstring text_;
    bool         auto_reset_ = false;

    graph() {}

    void activate() override { context::register_drawable(shared_from_this()); }

    void set_text(const std::wstring& value) override
    {
        auto                        temp = value;
        std::lock_guard<std::mutex> lock(mutex_);
        text_ = std::move(temp);
    }

    void set_value(const std::string& name, double value) override
    {
        lines_[name].set_value(static_cast<float>(value));
    }

    void set_tag(caspar::diagnostics::tag_severity /*severity*/, const std::string& name) override
    {
        lines_[name].set_tag();
    }

    void set_color(const std::string& name, int color) override { lines_[name].set_color(color); }

    void auto_reset() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto_reset_ = true;
    }

  private:
    void render(sf::RenderTarget& target, sf::RenderStates states) override
    {
        const size_t text_size   = 15;
        const size_t text_margin = 2;
        const size_t text_offset = (text_size + text_margin * 2) * 2;

        std::wstring text_str;
        bool         auto_reset;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            text_str   = text_;
            auto_reset = auto_reset_;
        }

        sf::Text text(get_default_font(), text_str.c_str(), text_size);
        text.setStyle(sf::Text::Italic);
        text.move({text_margin, text_margin});

        target.draw(text, states);

        if (context_.video_channel != -1) {
            auto ctx_str = std::to_string(context_.video_channel);

            if (context_.layer != -1)
                ctx_str += "-" + std::to_string(context_.layer);

            sf::Text context_text(get_default_font(), ctx_str, text_size);
            context_text.setStyle(sf::Text::Italic);
            context_text.move({RENDERING_WIDTH - text_margin - 5 - context_text.getLocalBounds().size.x, text_margin});

            target.draw(context_text, states);
        }

        float x_offset = text_margin;

        for (auto it = lines_.begin(); it != lines_.end(); ++it) {
            sf::Text line_text(get_default_font(), it->first, text_size);
            line_text.setPosition({x_offset, text_margin + text_offset / 2.0F});
            line_text.setFillColor(get_sfml_color(it->second.get_color()));
            target.draw(line_text, states);
            x_offset += line_text.getLocalBounds().size.x + text_margin * 2;
        }

        static const auto rect = []() {
            sf::RectangleShape r(sf::Vector2f(RENDERING_WIDTH, RENDERING_HEIGHT - 2));
            r.setFillColor(sf::Color(255, 255, 255, 51));
            r.setOutlineThickness(0.00f);
            r.move({0, 1});
            return r;
        }();
        target.draw(rect, states);

        states.transform.translate({0, text_offset})
            .scale({RENDERING_WIDTH,
                    RENDERING_HEIGHT *
                        (static_cast<float>(RENDERING_HEIGHT - text_offset) / static_cast<float>(RENDERING_HEIGHT))});

        static const sf::Color       guide_color(255, 255, 255, 127);
        static const sf::VertexArray middle_guide = []() {
            sf::VertexArray result(sf::PrimitiveType::LineStrip);
            result.append(sf::Vertex{sf::Vector2f(0.0f, 0.5f), guide_color});
            result.append(sf::Vertex{sf::Vector2f(1.0f, 0.5f), guide_color});
            return result;
        }();
        static const sf::VertexArray bottom_guide = []() {
            sf::VertexArray result(sf::PrimitiveType::LineStrip);
            result.append(sf::Vertex{sf::Vector2f(0.0f, 0.9f), guide_color});
            result.append(sf::Vertex{sf::Vector2f(1.0f, 0.9f), guide_color});
            return result;
        }();
        static const sf::VertexArray top_guide = []() {
            sf::VertexArray result(sf::PrimitiveType::LineStrip);
            result.append(sf::Vertex{sf::Vector2f(0.0f, 0.1f), guide_color});
            result.append(sf::Vertex{sf::Vector2f(1.0f, 0.1f), guide_color});
            return result;
        }();

        glEnable(GL_LINE_STIPPLE);
        glLineStipple(3, 0xAAAA);

        target.draw(middle_guide, states);
        target.draw(bottom_guide, states);
        target.draw(top_guide, states);

        glDisable(GL_LINE_STIPPLE);

        for (auto it = lines_.begin(); it != lines_.end(); ++it) {
            target.draw(it->second, states);
            if (auto_reset)
                it->second.set_value(0.0f);
        }
    }
};

void register_sink()
{
    caspar::diagnostics::spi::register_sink_factory([] { return spl::make_shared<graph>(); });
}

void show_graphs(bool value) { context::show(value); }

void shutdown() { context::shutdown(); }

}}}} // namespace caspar::core::diagnostics::osd
