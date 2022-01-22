#include <iostream>
#include <string>
#include <vector>

// https://github.com/preshing/FlexibleReflection/blob/part1/Reflect.h
//https://preshing.com/20180124/a-flexible-reflection-system-in-cpp-part-2/
// CppCon 2017: Arthur O'Dwyer “A Soupçon of SFINAE” https://www.youtube.com/watch?v=ybaE9qlhHvw
namespace Reflection {
	struct TypeDescriptor {
		std::string name;
		size_t size;

		TypeDescriptor(const char* name, size_t size) : name{ name }, size(size){}
		virtual ~TypeDescriptor() {}
		virtual std::string getFullName() const { return name; }
		virtual void dump(const void* obj, int indentLevel = 0) const = 0;
	};

	template <typename T>
	TypeDescriptor* getPrimitiveDescriptor();

	struct DefaultResolver {
		template <typename T> static char func(decltype(&T::Reflection));
		template <typename T> static int func(...);

		template <typename T> struct IsReflected {
			enum {
				value = (sizeof(func<T>(nullptr)) == sizeof(char))
			};
		};

		template <typename T, typename std::enable_if<IsReflected<T>::value, int>::type = 0>
		static TypeDescriptor* get()
		{
			return &T::Relection;
		}

		template <typename T, typename std::enable_if<!IsReflected<T>::value, int>::type = 0>
		static TypeDescriptor* get()
		{
			return getPrimitiveDescriptor<T>();
		}
	};


	template <typename T>
	struct TypeResolver {
		static TypeDescriptor* get() {
			return DefaultResolver::get<T>();
		}
	};

	struct TypeDescriptor_Struct : TypeDescriptor
	{
		struct Member {
			std::string name;
			size_t offset;
			TypeDescriptor* type;
		};


	};

}
struct Node {
	std::string key;
	std::vector<Node> children;
};
int main()
{
	return 0;

}