#include <functional>
#include <type_traits>
#include <cstring>
#include <memory>
#include <utility>


namespace trait {
    namespace __impl {
        // When TraitA is a base of TraitB,
        // __TraitTypeCheck<TraitA> can be
        // converted to __TraitTypeCheck<TraitB>
        // e.g. __TraitTypeCheck<A> = check ? super A
        template<typename Trait>
        struct __TraitTypeCheck {
            __TraitTypeCheck() = default;
            template<typename TraitB>
            __TraitTypeCheck(__TraitTypeCheck<TraitB>,
                             typename std::enable_if<std::is_base_of<TraitB, Trait>::value, int>::type __assert=0) {}
        };

        template<class TraitImpl_, bool has_impl_>
        struct __has_trait_impl_result {
            using TraitImpl = TraitImpl_;
            constexpr static bool value = has_impl_;
        };

        __has_trait_impl_result<void, false> __has_trait_impl(...);

        template<typename Trait_, typename Base_>
        auto __has_trait_impl(Trait_ *, Base_ *)->
            __has_trait_impl_result<decltype(__trait_impl(__TraitTypeCheck<Trait_>(), static_cast<Base_*>(nullptr))), true>;
        template<typename Base_, typename Trait_, typename ...OtherTraits>
        struct is_trait_h :
            public std::conditional_t<bool(is_trait_h<Base_, Trait_>::value),
                                      is_trait_h<Base_, OtherTraits...>,
                                      is_trait_h<Base_, Trait_>> {
        };

        template<typename Base_, typename Trait_>
        struct is_trait_h<Base_, Trait_> {
            using Trait = typename std::remove_reference<Trait_>::type;
            using Base = typename std::remove_reference<Base_>::type;
            using __result = decltype(__has_trait_impl(static_cast<Trait*>(nullptr), static_cast<Base*>(nullptr)));
            using TraitImpl = typename __result::TraitImpl;
            constexpr static bool value = __result::value;
        };

        template<typename ...TraitH>
        struct __is_trait_h_conj : public std::true_type {};

        template<typename FirstTrait>
        struct __is_trait_h_conj<FirstTrait> : public FirstTrait {};

        template<typename FirstTrait, typename ...TraitH>
        struct __is_trait_h_conj<FirstTrait, TraitH...> :
                public std::conditional_t<bool(FirstTrait::value), __is_trait_h_conj<TraitH...>, FirstTrait> {};

        template<typename Base_, typename Trait_, typename ...OtherTraits>
        constexpr bool is_trait = is_trait_h<Base_, Trait_, OtherTraits...>::value;


        template<typename Base_, typename Trait_, typename ...OtherTraits>
        using trait_assert = typename std::enable_if<is_trait_h<Base_, Trait_, OtherTraits...>::value, Base_>::type;

        template<class Trait_, class Self>
        struct __TraitImplBase: public Trait_ {
            using __Self = Self;
            using __Trait = Trait_;
            __Self &self;
            explicit __TraitImplBase(__Self &self_) noexcept : self(self_) {}
            explicit __TraitImplBase(__Self &&self_) noexcept : self(self_) {}
        };

        template<class Base>
        struct __TraitCastHelper {
            template<class TraitImpl_,
                     class TraitImpl=typename std::remove_reference<TraitImpl_>::type>
            typename std::remove_reference<decltype(((TraitImpl*)(nullptr))->self)>::type &operator()(TraitImpl_ &&trait) {
                using __Self=typename std::remove_reference<decltype(((TraitImpl*)(nullptr))->self)>::type;
                static_assert(std::is_same<Base, __Self>::value, "type mismatch");
                using __Trait=typename TraitImpl::Trait;
                static_assert(std::is_base_of<__TraitImplBase<__Trait, __Self>, TraitImpl>::value,
                              "invalid trait implementation");
                return trait.self;
            }

            template<class Trait>
            trait_assert<Base, Trait> &operator()(Trait &&trait) {
                using TraitImpl = typename is_trait_h<Base, Trait>::TraitImpl;
                return static_cast<TraitImpl&>(trait).self;
            }

        };

        template<class Base, class Trait>
        Base &cast(Trait &&trait) {
            return __TraitCastHelper<Base>()(std::forward<Trait>(trait));
        }

        struct __EmptyTraitTarget final {};

        template<class Trait>
        using TraitUPtr = std::unique_ptr<Trait, void(*)(Trait*)>;

        template<class Trait>
        struct TraitRef final {
            alignas(__TraitImplBase<Trait, __EmptyTraitTarget>) char buffer[sizeof(__TraitImplBase<Trait, __EmptyTraitTarget>)];
            template<class Base, class __assert=trait_assert<Base, Trait>>
            TraitRef(Base &&base) {
                using TraitImpl = typename is_trait_h<Base, Trait>::TraitImpl;
                static_assert(sizeof(TraitImpl) == sizeof(buffer) && alignof(TraitImpl) == alignof(__TraitImplBase<Trait, __EmptyTraitTarget>),
                              "cannot accept a non-standard trait: size/alignment not match");
                static_assert(std::is_trivially_destructible<TraitImpl>::value,
                              "cannot accept a non-standard trait: not trivially destructible");
                new(buffer) TraitImpl{std::forward<Base>(base)};
            }
            TraitRef(Trait &trait): TraitRef{reinterpret_cast<TraitRef&>(trait)} {}
            TraitRef(TraitUPtr<Trait> &ptr): TraitRef(reinterpret_cast<TraitRef&>(*ptr)) {}
            TraitRef(TraitRef &) = default;
            TraitRef(TraitRef &&) = default;
            operator Trait*() {
                return reinterpret_cast<Trait*>(buffer);
            }
            Trait& operator*() {
                return *reinterpret_cast<Trait*>(buffer);
            }
            Trait* operator->() {
                return reinterpret_cast<Trait*>(buffer);
            }

            template<class Base>
            Base &cast() {
                return trait::__impl::cast<Base>(**this);
            }
        };

        template<class Trait>
        struct __TraitCastHelper<TraitRef<Trait>> {
            TraitRef<Trait> &operator()(Trait &trait) {
                return reinterpret_cast<TraitRef<Trait>&>(trait);
            }

            TraitRef<Trait> &operator()(Trait &&trait) {
                return reinterpret_cast<TraitRef<Trait>&>(trait);
            }
        };

        template<class T>
        struct __TraitRefTest {
            using type=T;
        };

        template<class T>
        struct __TraitRefTest<TraitRef<T>> {
            using trait_type=T;
        };

        template<class Trait, class Base,
                 class __assert=typename __TraitRefTest<Base>::type,
                 class __not_ref=typename __TraitRefTest<typename is_trait_h<Base, Trait>::TraitImpl>::type>
        auto to_trait(Base &&value) {
            static_assert(is_trait<Base, Trait>, "trait not implemented for this type");
            using TraitImpl=typename is_trait_h<Base, Trait>::TraitImpl;
            return TraitImpl{std::forward<Base>(value)};
        }

        template<class Trait, class Trait2>
        std::enable_if_t<std::is_base_of<Trait, Trait2>::value, Trait2&> to_trait(TraitRef<Trait2> &value) {
            return *value;
        }

        template<class Trait, class Trait2>
        std::enable_if_t<std::is_base_of<Trait, Trait2>::value, Trait2&> &to_trait(TraitRef<Trait2> &&value) {
            return *value;
        }

        template<class Trait, class Trait2>
        std::enable_if_t<std::is_base_of<Trait, Trait2>::value, Trait2&> to_trait(TraitUPtr<Trait2> &value) {
            return *value;
        }

        template<class Trait, class Trait2>
        std::enable_if_t<std::is_base_of<Trait, Trait2>::value, Trait2&> &to_trait(TraitUPtr<Trait2> &&value) {
            return *value;
        }

        template<template<typename> class TraitTemplate,
                 typename Base>
        auto to_trait(Base &&value) {
            return to_trait<TraitTemplate<Base>>(std::forward<Base>(value));
        }

        template<class Trait_, class Base_>
        struct TraitUPtrDirect final {
            using Trait = typename is_trait_h<Base_, Trait_>::Trait;
            using Base = typename is_trait_h<Base_, Trait_>::Base;
            using TraitImpl = typename is_trait_h<Base_, Trait_>::TraitImpl;
            static_assert(sizeof(TraitImpl) == sizeof(TraitRef<Trait>) && alignof(TraitImpl) == alignof(TraitRef<Trait>),
                          "cannot accept a non-standard trait: size/alignment not match");
            static_assert(std::is_trivially_destructible<TraitImpl>::value,
                          "cannot accept a non-standard trait: not trivially destructible");
            static_assert(std::is_nothrow_constructible<TraitImpl, Base&>::value,
                          "cannot accept a non-standard trait: not trivially constructible");
            alignas(TraitImpl) char trait_buffer[sizeof(TraitImpl)];
            alignas(Base) char base_buffer[sizeof(Base)];
            template<class ...Args>
            TraitUPtrDirect(Args&& ...args) {
                new(base_buffer) Base{std::forward<Args>(args)...};
                new(trait_buffer) TraitImpl{*reinterpret_cast<Base*>(base_buffer)};
            }
            ~TraitUPtrDirect() {
                reinterpret_cast<TraitImpl*>(trait_buffer)->~TraitImpl();
                reinterpret_cast<Base*>(base_buffer)->~Base();
            }
            TraitUPtrDirect(TraitUPtrDirect&) = delete;
            TraitUPtrDirect(TraitUPtrDirect&&) = delete;
            TraitUPtrDirect& operator=(TraitUPtrDirect&) = delete;
            TraitUPtrDirect& operator=(TraitUPtrDirect&&) = delete;
            template<class ...Args>
            static Trait* make(Args&& ...args) {
                return reinterpret_cast<Trait*>(new TraitUPtrDirect{std::forward<Args>(args)...});
            }
            static void deleter(Trait *trait) {
                delete reinterpret_cast<TraitUPtrDirect*>(trait);
            }
        };

        template<class Trait_, class Base_, class Deleter>
        struct TraitUPtrUPtr final {
            using Trait = typename is_trait_h<Base_, Trait_>::Trait;
            using Base = typename is_trait_h<Base_, Trait_>::Base;
            using TraitImpl = typename is_trait_h<Base_, Trait_>::TraitImpl;
            static_assert(sizeof(TraitImpl) == sizeof(TraitRef<Trait>) && alignof(TraitImpl) == alignof(TraitRef<Trait>),
                          "cannot accept a non-standard trait: size/alignment not match");
            static_assert(std::is_trivially_destructible<TraitImpl>::value,
                          "cannot accept a non-standard trait: not trivially destructible");
            alignas(TraitImpl) char trait_buffer[sizeof(TraitImpl)];
            alignas(std::unique_ptr<Base, Deleter>) char base_buffer[sizeof(std::unique_ptr<Base, Deleter>)];
            TraitUPtrUPtr(std::unique_ptr<Base, Deleter> &&value) {
                new (base_buffer) std::unique_ptr<Base, Deleter>(std::move(value));
                new (trait_buffer) TraitImpl{**reinterpret_cast<std::unique_ptr<Base, Deleter>*>(base_buffer)};
            }
            ~TraitUPtrUPtr() {
                reinterpret_cast<TraitImpl*>(trait_buffer)->~TraitImpl();
                reinterpret_cast<std::unique_ptr<Base, Deleter>*>(base_buffer)->~unique_ptr();
            }
            TraitUPtrUPtr(TraitUPtrUPtr&) = delete;
            TraitUPtrUPtr(TraitUPtrUPtr&&) = delete;
            TraitUPtrUPtr& operator=(TraitUPtrUPtr&) = delete;
            TraitUPtrUPtr& operator=(TraitUPtrUPtr&&) = delete;
            static Trait* make(std::unique_ptr<Base, Deleter> &&value) {
                return reinterpret_cast<Trait*>(new TraitUPtrUPtr{std::move(value)});
            }
            static void deleter(Trait *trait) {
                delete reinterpret_cast<TraitUPtrUPtr*>(trait);
            }
        };

        template<class Trait, class Base>
        TraitUPtr<Trait> own(Base &&value) {
            static_assert(is_trait<Base, Trait>, "trait not implemented for this type");
            using UPtr=TraitUPtrDirect<Trait, Base>;
            return TraitUPtr<Trait>{UPtr::make(std::forward<Base>(value)), UPtr::deleter};
        }

        template<class Trait, class Base, class Deleter>
        std::enable_if_t<(is_trait_h<Base, Trait>::value ||
                          !is_trait_h<std::unique_ptr<Base, Deleter>, Trait>::value),
                          TraitUPtr<Trait>> own(std::unique_ptr<Base, Deleter> &&value) {
            static_assert(is_trait<Base, Trait>, "trait not implemented for this type");
            using UPtr=TraitUPtrUPtr<Trait, Base, Deleter>;
            return TraitUPtr<Trait>{UPtr::make(std::move(value)), UPtr::deleter};
        }

        template<class Trait, class Base, class ...Args>
        TraitUPtr<Trait> make(Args &&...args) {
            static_assert(is_trait<Base, Trait>, "trait not implemented for this type");
            using UPtr=TraitUPtrDirect<Trait, Base>;
            return TraitUPtr<Trait>{UPtr::make(std::forward<Args>(args)...), UPtr::deleter};
        }
    }
    using __impl::is_trait;
    using __impl::to_trait;
    using __impl::cast;
    using __impl::TraitRef;
    using __impl::TraitUPtr;
    using __impl::own;
    using __impl::make;
    using __impl::trait_assert;
    using __impl::is_trait_h;
}

template<typename Base, typename Trait, typename R, typename ...Args,
         typename __assert=trait::trait_assert<Base, Trait>>
auto operator->*(Base &&base, R (Trait::*ptr)(Args...)) {
    return [&base, ptr](Args &&...args)->decltype(auto) {
        return (trait::to_trait<Trait>(std::forward<Base>(base)).*ptr)(std::forward<Args>(args)...);
    };
}

template<class Trait, class Self, class ...RequiredTraits>
struct __TraitImpl final: public trait::__impl::__TraitImplBase<Trait, Self> {
    using __noimpl = decltype(Trait());
};

#define TRAIT_COMMA ,
#define TRAIT_PARA(...) __VA_ARGS__

#define IMPL_TRAIT_FOR_CLASS(TraitCls, BaseCls) \
__TraitImpl<TraitCls, BaseCls> __trait_impl(::trait::__impl::__TraitTypeCheck<TraitCls>, ::std::add_pointer_t<BaseCls>);\
template<> \
struct __TraitImpl<TraitCls, BaseCls> final : public ::trait::__impl::__TraitImplBase<TraitCls, BaseCls>

#define IMPL_TRAIT_FOR_TRAIT(TraitCls, ...) \
template<typename Self> \
typename ::std::enable_if<::trait::is_trait_h<Self, __VA_ARGS__>::value,\
                          __TraitImpl<TraitCls, Self, ::trait::is_trait_h<Self, __VA_ARGS__>>>::type \
__trait_impl(::trait::__impl::__TraitTypeCheck<TraitCls>, Self*);\
template<class Self> \
struct __TraitImpl<TraitCls, Self, ::trait::is_trait_h<Self, __VA_ARGS__>> final : public ::trait::__impl::__TraitImplBase<TraitCls, Self>

#define IMPL_TRAIT_FOR_GENERIC(TemplateExpr, TraitCls, BaseCls, ...) \
template<TemplateExpr> \
typename ::std::enable_if<::trait::__impl::__is_trait_h_conj<__VA_ARGS__>::value,\
                          __TraitImpl<TraitCls, BaseCls, ::trait::__impl::__is_trait_h_conj<__VA_ARGS__>>>::type \
__trait_impl(::trait::__impl::__TraitTypeCheck<TraitCls>, BaseCls*);\
template<TemplateExpr> \
struct __TraitImpl<TraitCls, BaseCls, ::trait::__impl::__is_trait_h_conj<__VA_ARGS__>> final : public ::trait::__impl::__TraitImplBase<TraitCls, BaseCls>

#define TRAIT_FOR_CLASS_SELF \
    using __TraitImplBase = __TraitImpl::__TraitImplBase;\
    using __TraitImplBase::__TraitImplBase;\
    using Trait = typename __TraitImplBase::__Trait;\
    using Self = typename __TraitImplBase::__Self;\
    using __TraitImplBase::self;

#define TRAIT_FOR_TRAIT_SELF(TraitCls) \
    using __TraitImplBase = ::trait::__impl::__TraitImplBase<TraitCls, Self>;\
    using __TraitImplBase::__TraitImplBase;\
    using Trait = typename __TraitImplBase::__Trait;\
    using __TraitImplBase::self;

#define TRAIT_FOR_GENERIC_SELF(TraitCls, BaseCls) \
    using __TraitImplBase = ::trait::__impl::__TraitImplBase<TraitCls, BaseCls>;\
    using __TraitImplBase::__TraitImplBase;\
    using Trait = typename __TraitImplBase::__Trait;\
    using __TraitImplBase::self;

#define TRAIT_BOUND(Trait, ...) trait::is_trait_h<Trait, __VA_ARGS__>


template<class Trait, class Base, class Trait2>
std::enable_if_t<std::is_base_of<Trait2, Trait>::value, __TraitImpl<Trait, Base>>
    __trait_impl(::trait::__impl::__TraitTypeCheck<Trait2>, __TraitImpl<Trait, Base>*);


template<class Trait, class Trait2>
std::enable_if_t<std::is_base_of<Trait2, Trait>::value, trait::TraitRef<Trait>>
    __trait_impl(::trait::__impl::__TraitTypeCheck<Trait2>, trait::TraitRef<Trait>*);


template<class Trait, class Trait2>
std::enable_if_t<std::is_base_of<Trait2, Trait>::value, trait::TraitRef<Trait>>
    __trait_impl(::trait::__impl::__TraitTypeCheck<Trait2>, trait::TraitUPtr<Trait>*);
