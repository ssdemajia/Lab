#include <iostream>
#include <string>
#include <vector>
#include <functional>

// https://github.com/preshing/FlexibleReflection/blob/part1/Reflect.h
//https://preshing.com/20180124/a-flexible-reflection-system-in-cpp-part-2/
// CppCon 2017: Arthur O'Dwyer “A Soupçon of SFINAE” https://www.youtube.com/watch?v=ybaE9qlhHvw
namespace Reflection {
	template<class T>
	class Singleton
	{
	public:
		using object_type = T;
	public:
		static object_type* GetInstance()
		{
			static object_type _instance;
			return &_instance;
		}

	};

	using CreateObjectFunc = std::function<void* ()>;

	struct CreateObjectFuncClass {
		explicit CreateObjectFuncClass(CreateObjectFunc func) : create_func(func) {}
		CreateObjectFunc create_func;
	};


	class ObjectFactory : public Singleton<ObjectFactory>
	{
	public:
		void* CreateObject(const std::string& className)
		{
			CreateObjectFunc objFunc(nullptr);
			if (create_funcs_.find(className) != create_funcs_.end())
				objFunc = create_funcs_.find(className)->second->create_func;

			if (objFunc == nullptr)
				return nullptr;

			//   REGISTER_CLASS new className 
			return objFunc();
		}

		void RegisterObject(const std::string& class_name, CreateObjectFunc func) {
			auto it = create_funcs_.find(class_name);
			if (it != create_funcs_.end())
				create_funcs_[class_name]->create_func = func;
			else
				create_funcs_.emplace(class_name, new CreateObjectFuncClass(func));
		}

		~ObjectFactory() {
			for (auto it : create_funcs_)
			{
				if (it.second != nullptr)
				{
					delete it.second;
					it.second = nullptr;
				}
			}
			create_funcs_.clear();
		}
	private:
		std::unordered_map<std::string, CreateObjectFuncClass* > create_funcs_;
	};

	// 类型描述符
	struct TypeDescriptor {
		const char* name;
		size_t size;

		TypeDescriptor(const char* name, size_t size) : name{ name }, size(size){}
		virtual ~TypeDescriptor() {}
		virtual std::string getFullName() const { return name; }
		virtual void dump(const void* obj, int indentLevel = 0) const = 0;
	};

	template <typename T>
	TypeDescriptor* getPrimitiveDescriptor();

	// 获取类型的描述符
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
			return &T::Reflection;
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

		std::vector<Member> members;

		TypeDescriptor_Struct(void(*init)(TypeDescriptor_Struct*)) : TypeDescriptor{ nullptr, 0 }
		{
			init(this);
		}

		TypeDescriptor_Struct(const char* name, size_t size, const std::initializer_list<Member>& initList)
			: TypeDescriptor(name, size), members(initList)
		{}

		virtual void dump(const void* obj, int indentLevel = 0) const
		{
			std::cout << "{" << std::endl;
			for (const Member& member : members)
			{
				std::cout << std::string(4 * (indentLevel + 1), ' ') << member.name << " = ";
				member.type->dump((char*)obj + member.offset, indentLevel + 1);
				std::cout << std::endl;
			}
			std::cout << std::string(4 * indentLevel, ' ') << "}" << std::endl;
		}
	};



#define REFLECT() \
	friend struct Reflection::DefaultResolver; \
	static Reflection::TypeDescriptor_Struct Reflection; \
	static void initReflection(Reflection::TypeDescriptor_Struct*);

#define REFLECT_STRUCT_BEGIN(type) \
	Reflection::TypeDescriptor_Struct type::Reflection{ initReflection };\
	void type::initReflection(Reflection::TypeDescriptor_Struct* typeDesc) {\
		using T = type; \
		typeDesc->name = #type; \
		typeDesc->size = sizeof(T); \
		typeDesc->members = {

#define REFLECT_STRUCT_MEMBER(name) \
		{#name, offsetof(T, name), Reflection::TypeResolver<decltype(T::name)>::get()},

#define REFLECT_STRUCT_END() \
		};\
	}



	struct TypeDescriptor_Vector : TypeDescriptor {
		TypeDescriptor* itemType;
		size_t(*getSize)(const void*);
		const void* (*getItem)(const void*, size_t);
		template <typename ItemType>
		TypeDescriptor_Vector(ItemType*)
			: TypeDescriptor{ "std::vector<>", sizeof(std::vector<ItemType>) },
			itemType{ TypeResolver<ItemType>::get() } {
			getSize = [](const void* vecPtr) -> size_t {
				const auto& vec = *(const std::vector<ItemType>*) vecPtr;
				return vec.size();
			};
			getItem = [](const void* vecPtr, size_t index) -> const void* {
				const auto& vec = *(const std::vector<ItemType>*) vecPtr;
				return &vec[index];
			};
		}
		virtual std::string getFullName() const override {
			return std::string("std::vector<") + itemType->getFullName() + ">";
		}
		virtual void dump(const void* obj, int indentLevel) const override {
			size_t numItems = getSize(obj);
			std::cout << getFullName();
			if (numItems == 0) {
				std::cout << "{}";
			}
			else {
				std::cout << "{" << std::endl;
				for (size_t index = 0; index < numItems; index++) {
					std::cout << std::string(4 * (indentLevel + 1), ' ') << "[" << index << "] ";
					itemType->dump(getItem(obj, index), indentLevel + 1);
					std::cout << std::endl;
				}
				std::cout << std::string(4 * indentLevel, ' ') << "}";
			}
		}
	};

	// Partially specialize TypeResolver<> for std::vectors:
	template <typename T>
	class TypeResolver<std::vector<T>> {
	public:
		static TypeDescriptor* get() {
			static TypeDescriptor_Vector typeDesc{ (T*) nullptr };
			return &typeDesc;
		}
	};


	//--------------------------------------------------------
	// A type descriptor for int
	//--------------------------------------------------------

	struct TypeDescriptor_Int : TypeDescriptor {
		TypeDescriptor_Int() : TypeDescriptor{ "int", sizeof(int) } {
		}
		virtual void dump(const void* obj, int /* unused */) const override {
			std::cout << "int{" << *(const int*)obj << "}";
		}
	};

	template <>
	TypeDescriptor* getPrimitiveDescriptor<int>() {
		static TypeDescriptor_Int typeDesc;
		return &typeDesc;
	}

	//--------------------------------------------------------
	// A type descriptor for std::string
	//--------------------------------------------------------

	struct TypeDescriptor_StdString : TypeDescriptor {
		TypeDescriptor_StdString() : TypeDescriptor{ "std::string", sizeof(std::string) } {
		}
		virtual void dump(const void* obj, int /* unused */) const override {
			std::cout << "std::string{\"" << *(const std::string*)obj << "\"}";
		}
	};

	template <>
	TypeDescriptor* getPrimitiveDescriptor<std::string>() {
		static TypeDescriptor_StdString typeDesc;
		return &typeDesc;
	}

#define REGISTERPANELCLASS(className) \
	class className##Helper{ \
	public: \
		className##Helper() \
		{ \
			Reflection::ObjectFactory::GetInstance()->RegisterObject(#className,[]() \
			{ \
				auto * obj = new className(); \
				return obj; \
			}); \
		} \
	}; \
	className##Helper g_##className##_helper;// helper RegisterObject 
}



struct Node {
	REFLECT()
	
	std::string key;
	std::vector<Node> children;
	virtual void print() { std::cout << "Node" << std::endl; }
};

struct Widget : public Node {
	REFLECT()

	int i = 1;
	virtual void print() { std::cout << "Widget" << std::endl; }
};
REFLECT_STRUCT_BEGIN(Node)
REFLECT_STRUCT_MEMBER(key)
REFLECT_STRUCT_MEMBER(children)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Widget)
REFLECT_STRUCT_MEMBER(i)
REFLECT_STRUCT_END()

REGISTERPANELCLASS(Node)
REGISTERPANELCLASS(Widget)
int main()
{
	Node n;
	n.key = "1233ss";
	Node n2;
	n2.key = "sb";
	n.children.push_back(n2);
	auto descriptor = Reflection::TypeResolver<Node>::get();
	descriptor->dump(&n);
	Node* widget = (Node*)Reflection::ObjectFactory::GetInstance()->CreateObject("Widget");
	widget->print();
	descriptor->dump(widget);
	return 0;

}