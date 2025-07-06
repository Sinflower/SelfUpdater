#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>


#if defined(_MSC_VER)
// MSVC compiler
#define CPP_VERSION _MSVC_LANG
#else
// GCC, Clang, or other standards-compliant compilers
#define CPP_VERSION __cplusplus
#endif

#if CPP_VERSION < 201703L
#error "This program requires C++17 or later, for MSVC use /std:c++17 or later"
#endif

// Compile using MSVC:
// cl /std:c++20 /O2 /DNDEBUG IncResVer.cpp

static bool g_msFormat = false;

void update_line(std::string& line, const std::regex& pattern, const bool& is_string_format)
{
	std::smatch match;
	if (std::regex_search(line, match, pattern))
	{
		std::array<int32_t, 4> version = { 0, 0, 0, 0 };

		// Check if the required amount of groups is present
		if (match.size() < 6)
		{
			std::cerr << "Error: Invalid version format in line: " << line << std::endl;
			return;
		}

		std::string prefix = match[1].str();

		version[0] = std::stoi(match[2]);
		version[1] = std::stoi(match[3]);
		version[2] = std::stoi(match[4]);
		version[3] = std::stoi(match[5]);

		if (g_msFormat)
			version[2] += 1;
		else
			version[3] += 1;

		std::ostringstream updated;
		if (is_string_format)
		{
			std::string trailing = match[6].matched ? match[6].str() : "";
			updated << prefix << version[0] << '.' << version[1] << '.' << version[2] << '.' << version[3] << trailing << "\"";
		}
		else
			updated << prefix << version[0] << ',' << version[1] << ',' << version[2] << ',' << version[3];

		line = updated.str();
	}
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cerr << "Usage: " << argv[0] << " <filename.rc> [MS_FORMAT]" << std::endl;
		return 1;
	}

	if (argc >= 3 && std::string(argv[2]) == "MS_FORMAT")
		g_msFormat = true;

	std::string filename = argv[1];

	std::ifstream infile(filename);
	if (!infile)
	{
		std::cerr << "Error: Cannot open file: " << filename << std::endl;
		return 1;
	}

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(infile, line))
		lines.push_back(line);

	infile.close();

	std::regex numeric_pattern(R"((.*\b(?!FILEVERSION|PRODUCTVERSION)\s+)(\d+),(\d+),(\d+),(\d+))");
	std::regex string_pattern(R"|((.*\bVALUE\s+"(?:FileVersion|ProductVersion)",\s*"\s*)(\d+)\.(\d+)\.(\d+)\.(\d+)(\\0)?")|");

	for (std::string& l : lines)
	{
		update_line(l, numeric_pattern, false);
		update_line(l, string_pattern, true);
	}

	const std::string backup_filename = filename + ".bak";
	std::filesystem::copy_file(filename, backup_filename, std::filesystem::copy_options::overwrite_existing);

	std::ofstream outfile(filename);
	if (!outfile)
	{
		std::cerr << "Error: Cannot write to file: " << filename << std::endl;
		return 2;
	}

	for (const auto& l : lines)
		outfile << l << std::endl;

	std::cout << "Build version incremented successfully in " << filename << std::endl;
	return 0;
}