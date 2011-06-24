#include <EASTL\vector.h>
#include <iostream>

int main() {
	eastl::vector<int> v;
	v.push_back(1);
	std::cout << v[0] << std::endl;
	v.clear();
	return 0;
}