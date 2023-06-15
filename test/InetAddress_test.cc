#include <iostream>
#include "../InetAddress.h"

int main() {
    InetAddress address(9090, "198.162.1.1");
    std::cout << address.toIp() << std::endl;
    std::cout << address.toPort() << std::endl;
    std::cout << address.toIpPort() << std::endl;

    InetAddress address2(8888);
    std::cout << address2.toIp() << std::endl;
    std::cout << address2.toPort() << std::endl;
    std::cout << address2.toIpPort() << std::endl;

    return 0;
}