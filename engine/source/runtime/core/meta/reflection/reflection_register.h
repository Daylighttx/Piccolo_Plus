#pragma once
namespace Piccolo
{
    // reflection_register.h：反射注册入口。metaRegister() 在引擎启动时调用，
    // 把 _generated 下所有类型的字段/构造/读写函数登记进 TypeMeta（编辑器属性面板与序列化都靠它）。
    namespace Reflection
    {
        class TypeMetaRegister
        {
        public:
            static void metaRegister();
            static void metaUnregister();
        };
    } // namespace Reflection
} // namespace Piccolo