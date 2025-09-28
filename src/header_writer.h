#ifndef SILLY_PACKER_HEADER_WRITER_H
#define SILLY_PACKER_HEADER_WRITER_H

#include <cstdint>
#include <filesystem>
#include <fstream>

class header_writer {
public:
  header_writer(const std::filesystem::path& path, const std::string& guard,
                const std::string& spacename = "", bool use_stdint = true,
                bool use_raylib = false);
  ~header_writer();

  bool is_open() const;
  bool using_stdint() const;
  const std::string& byte_type() const;

  void write(const std::string& data);
  void write_variable(const std::string& type, const std::string& name,
                      const std::string& value, bool constant = true);
  void write_byte_array(const std::string& name, const std::uint8_t* data,
                        std::size_t size, bool constant = true);

  void close();

private:
  std::ofstream _fstream;
  bool _using_raylib;
  bool _using_stdint;
  bool _has_namespace;
  std::string _byte_type;
  bool _is_closed = false;
};

#endif
