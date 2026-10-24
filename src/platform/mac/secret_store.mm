// platform/mac/secret_store.mm
#include "platform/platform.hpp"
#import <Foundation/Foundation.h>
#import <Security/Security.h>

namespace
{
    NSString* const SERVICE = @"com.elf222.keyguide";

    NSMutableDictionary* SecretQuery(std::string_view name)
    {
        NSString* account = [[NSString alloc] initWithBytes:name.data()
                                                     length:name.size()
                                                   encoding:NSUTF8StringEncoding];
        return [@{
            (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService : SERVICE,
            (__bridge id)kSecAttrAccount : account ?: @"",
        } mutableCopy];
    }
} // namespace

namespace Platform
{
    std::string LoadSecret(std::string_view name)
    {
        NSMutableDictionary* query = SecretQuery(name);
        query[(__bridge id)kSecReturnData] = @YES;
        query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
        CFTypeRef result = nullptr;
        if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &result) != errSecSuccess ||
            !result)
        {
            return {};
        }
        NSData* data = (__bridge_transfer NSData*)result;
        return std::string(static_cast<const char*>([data bytes]), [data length]);
    }

    bool StoreSecret(std::string_view name, std::string_view value)
    {
        NSData* data = [NSData dataWithBytes:value.data() length:value.size()];
        OSStatus status =
            SecItemUpdate((__bridge CFDictionaryRef)SecretQuery(name),
                          (__bridge CFDictionaryRef) @{(__bridge id)kSecValueData : data});
        if (status == errSecItemNotFound)
        {
            NSMutableDictionary* item = SecretQuery(name);
            item[(__bridge id)kSecValueData] = data;
            item[(__bridge id)kSecAttrLabel] = @"KeyGuide credential";
            status = SecItemAdd((__bridge CFDictionaryRef)item, nullptr);
        }
        return status == errSecSuccess;
    }

    bool DeleteSecret(std::string_view name)
    {
        const OSStatus status = SecItemDelete((__bridge CFDictionaryRef)SecretQuery(name));
        return status == errSecSuccess || status == errSecItemNotFound;
    }
} // namespace Platform
