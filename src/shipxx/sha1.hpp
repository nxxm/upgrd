#ifndef SHA1_HPP
#define SHA1_HPP

//TODO: move in some pre lib
#include <array>
#include <boost/uuid/detail/sha1.hpp>

namespace shipxx::sha1 {
  std::array<uint32_t, 5> hash(const std::string& p_arg) {
      boost::uuids::detail::sha1 sha1;
      sha1.process_bytes(p_arg.data(), p_arg.size());
      uint32_t hash[5] = {0};
      sha1.get_digest(hash);

      return std::array<uint32_t, 5>{hash[0],hash[1],hash[2],hash[3],hash[4]};
  }

  std::string to_string(std::array<uint32_t, 5> hash) {
    char buf[41] = {0};

    for (int i = 0; i < 5; i++) {
        std::sprintf(buf + (i << 3), "%08x", hash[i]);
    }

    return std::string(buf);
  }
}

#endif
