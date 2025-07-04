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

module;

#include <ffmpeg/util/av_util.h>

#include <memory>
#include <string>

#include <boost/filesystem.hpp>

export module caspar.modules.image.util.loader;

namespace caspar { namespace image {

export std::shared_ptr<AVFrame> load_image(const std::wstring& filename);
export std::shared_ptr<AVFrame> load_from_memory(std::vector<unsigned char> image_data);

export bool is_valid_file(const boost::filesystem::path& filename);

}} // namespace caspar::image
