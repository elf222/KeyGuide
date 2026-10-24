// elfTypesRL/macros.hpp

#if defined(_DEBUG)
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif