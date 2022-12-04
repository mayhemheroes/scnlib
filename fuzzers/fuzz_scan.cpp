#include <scn/scn.h>
#include <string>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <iostream>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    std::string word;
    std::string buf(reinterpret_cast<const char *>(data), size);
    buf.push_back('\0');
    auto result = scn::scan(buf, "{}", word);
    
    // std::cout can damage the fuzz perfomance
    // but considering the library and methods it is fuzzing seems reasaonable
    std::cout << word << '\n';                      
    std::cout << result.range_as_string() << '\n';  
    return 0;
}
