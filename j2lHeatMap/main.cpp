#include <algorithm>
#include <cstdint>
#include <fstream>
#include <vector>
#include <zlib.h>
#include "lodepng.h"

bool read_decompressed(std::ifstream& input, std::vector<unsigned char>& output, uLong insize, uLongf outsize) {
	std::vector<char> buffer(insize);
	output.resize(outsize);
	input.read(buffer.data(), insize);
	auto in = reinterpret_cast<unsigned char*>(buffer.data());
	return uncompress(output.data(), &outsize, in, insize) == 0;
}

struct layer_info {
	bool has_tiles {};
	int width {};
	int real_width {};
	int height {};
};

int main(int argc, char* argv[]) {
	if (argc != 2)
		return 1;
	std::vector<unsigned char> general_data;
	std::vector<unsigned char> word_layout;
	{
		std::ifstream input(argv[1], std::ios_base::binary);
		input.ignore(230);
		std::uint32_t comp_sizes[4];
		std::uint32_t uncomp_sizes[4];
		for (int i = 0; i < 4; i++) {
			input.read(reinterpret_cast<char*>(&comp_sizes[i]), 4);
			input.read(reinterpret_cast<char*>(&uncomp_sizes[i]), 4);
		}
		if (!input.good())
			return 1;
		read_decompressed(input, general_data, comp_sizes[0], uncomp_sizes[0]);
		input.ignore((std::streamsize)comp_sizes[1] + comp_sizes[2]);
		read_decompressed(input, word_layout, comp_sizes[3], uncomp_sizes[3]);
		if (!input.good())
			return 1;
	}
	layer_info layers[8];
	for (std::size_t i = 0; i < 8; i++) {
		auto& layer = layers[i];
		layers[i].has_tiles = general_data[8443 + i];
		layers[i].width = reinterpret_cast<std::uint32_t&>(general_data[8451 + i * 4]);
		layers[i].real_width = reinterpret_cast<std::uint32_t&>(general_data[8483 + i * 4]);
		layers[i].height = reinterpret_cast<std::uint32_t&>(general_data[8515 + i * 4]);
	}
	std::vector<int> occurences(0x10000);
	for (std::size_t i = 0; i < word_layout.size(); i += 2) {
		occurences[word_layout[i] | word_layout[i + 1] << 8]++;
	}
	std::vector<unsigned char> palette(1024);
	{
		std::ifstream input("palette.pal", std::ios_base::binary);
		input.ignore(4);
		input.read(reinterpret_cast<char*>(palette.data()), 1024);
	}
	for (int i = 3; i < 1024; i += 4) {
		palette[i] = 255;
	}
	lodepng::State state;
	state.encoder.force_palette = true;
	state.info_raw.colortype = LCT_PALETTE;
	state.info_raw.palette = palette.data();
	state.info_raw.palettesize = 256;
	state.info_png.color.colortype = LCT_PALETTE;
	state.info_png.color.palette = palette.data();
	state.info_png.color.palettesize = 256;
	std::string name_prefix = argv[1];
	auto dot_index = name_prefix.rfind('.');
	if (dot_index != std::string::npos)
		name_prefix.erase(dot_index);
	name_prefix += "-layer_";
	auto word_it = word_layout.begin();
	for (int i = 0; i < 8; i++) {
		const auto& layer = layers[i];
		if (layer.has_tiles) {
			std::vector<unsigned char> image((std::size_t)layer.height * layer.real_width);
			auto image_it = image.begin();
			for (int y = 0; y < layer.height; y++) {
				for (int x = 0; x < layer.real_width; x += 4) {
					unsigned char color = std::min(255, occurences[word_it[0] | word_it[1] << 8]);
					for (int j = std::min(4, layer.real_width - x); j--;) {
						*image_it = color;
						++image_it;
					}
					word_it += 2;
				}
			}
			std::vector<unsigned char> out;
			lodepng::encode(out, image, layer.real_width, layer.height, state);
			lodepng::save_file(out, name_prefix + (char)('1' + i) + ".png");
		}
	}
	state.info_raw.palette = nullptr;
	state.info_png.color.palette = nullptr;
	return 0;
}
