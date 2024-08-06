#include <drogon/version.h>
#include <iostream>
#include <sstream>

std::string ver_string(int a, int b, int c) {
    std::ostringstream ss;
    ss << a << '.' << b << '.' << c;
    return ss.str();
}

void printInfo() {
    std::string true_cxx =
#ifdef __clang__
        "clang++";
#else
        "g++";
#endif
    std::string true_cxx_ver =
#ifdef __clang__
        ver_string(__clang_major__, __clang_minor__, __clang_patchlevel__);
#else
        ver_string(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif

    std::cout<<"C++ version: "<<cppv<<std::endl;
    std::cout<<"C++ compiler version: "<<true_cxx<<" "<<true_cxx_ver<<std::endl;
    std::cout<<"TRANTOR version: "<<TRANTOR_VERSION<<std::endl;
    std::cout<<"DROGON version: "<<DROGON_VERSION<<std::endl;
    std::cout<<"Server app version: "<<appversion<<std::endl;

#ifdef APP_DEBUG
    std::cout<<"\033[1m \033[1;31m !!! Running in Debug mode !!! \033[0m"<<std::endl;
#endif
}
