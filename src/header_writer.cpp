#include "header_writer.h"
#include <format>
#include <sstream>
#include <stdexcept>

header_writer::header_writer(const std::filesystem::path& path,
                             const std::string& guard,
                             const std::string& spacename, bool use_raylib)
    : _fstream(path), _header_path(path), _using_raylib(use_raylib),
      _has_namespace(false) {
  if (!_fstream.is_open()) {
    throw std::runtime_error("Could not open file");
  }

  write(std::format("#ifndef {0}\n#define {0}\n", guard));
  write("#include<array>\n");
  write("#include <cstdint>\n");
  write("#include <cstddef>\n");
  _byte_type = "std::uint8_t";

  if (_using_raylib)
    write("#include <raylib.h>\n");

  if (spacename != "") {
    write(std::format("namespace {} {{", spacename));
    _has_namespace = true;
  }
}

header_writer::~header_writer() { header_writer::close(); }

bool header_writer::is_open() const { return _fstream.is_open(); }

bool header_writer::using_raylib() const { return _using_raylib; }

bool header_writer::using_namespace() const { return _has_namespace; }

const std::string& header_writer::byte_type() const { return _byte_type; }

std::filesystem::path header_writer::get_path() const { return _header_path; }

void header_writer::write(const std::string& data) {
  _fstream.write(data.c_str(), data.size());
}

void header_writer::write_variable(const std::string& type,
                                   const std::string& name,
                                   const std::string& value, bool constant) {
  std::string constant_string = "";
  if (constant) {
    constant_string = "constexpr ";
  }
  const std::string type_string = std::format("inline {}", type);
  write(std::format("{}{} {}={};", constant_string, type_string, name, value));
}

void header_writer::write_byte_array(const std::string& name,
                                     const std::uint8_t* data, std::size_t size,
                                     bool constant) {
  std::stringstream bytes_stream;
  for (std::size_t i = 0; i < size; i++) {
    bytes_stream << static_cast<unsigned int>(data[i]) << ',';
  }

  std::string constant_string = "";
  if (constant) {
    constant_string = "constexpr ";
  }
  std::string type_string =
      std::format("inline std::array<{},{}>", _byte_type, size);
  write(std::format("{}{} {}={{{}}};", constant_string, type_string, name,
                    bytes_stream.str()));
}

void header_writer::close() {
  // prevent double destruction
  if (!_is_closed) {
    if (_has_namespace) {
      write("}");
    }
    write("\n#endif");
    _fstream.close();
    _is_closed = true;
  }
}
