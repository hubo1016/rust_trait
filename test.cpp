#include <functional>
#include <type_traits>
#include <iostream>
#include "rust_trait.h"


struct TraitA {
    virtual void test() = 0;
    virtual void test2(int a) = 0;
};


namespace testa {
    struct Test {
        Test() {
            std::cout<<"Test constructed, "<<this<<std::endl;
        }
        Test(Test &&t) {
            std::cout<<"Test moved, "<<&t<<" "<<this<<std::endl;
        }
        Test(const Test &t) {
            std::cout<<"Test copied, "<<&t<<" "<<this<<std::endl;
        }
        ~Test() {
            std::cout<<"Test destructed, "<<this<<std::endl;
        }
        void test_call() {
            std::cout<<"test call "<<this<<std::endl;
        }
    };

    struct Test2 {
        void test() {
            std::cout<<"test2 test "<<this<<std::endl;
        }
    };
}

IMPL_TRAIT_FOR_CLASS(TraitA, testa::Test) {
    TRAIT_FOR_CLASS_SELF;
    void test() override {
        std::cout<<"test"<<std::endl;
    }
    void test2(int a) override {
        std::cout<<"testint "<<a<<std::endl;
    }
};

struct TraitB {
    virtual void test3() = 0;
};

IMPL_TRAIT_FOR_TRAIT(TraitB, TraitA) {
    TRAIT_FOR_TRAIT_SELF(TraitB);
    void test3() override {
        std::cout<<"test3 from traitB for traitA"<<std::endl;
        trait::to_trait<TraitA>(self).test();
    }
};

template<class Self>
struct TraitC {
    virtual Self clone() = 0;
};

IMPL_TRAIT_FOR_CLASS(TraitC<testa::Test2>, testa::Test2) {
    TRAIT_FOR_CLASS_SELF;
    Self clone() override {
        return Self{self};
    }
};

IMPL_TRAIT_FOR_TRAIT(TraitB, TraitC<Self>) {
    TRAIT_FOR_TRAIT_SELF(TraitB);
    void test3() override {
        std::cout<<"test traitB for traitC"<<std::endl;
    }
};

struct Test3 {
};

struct TraitD: public TraitA {
    virtual int test4() {
        return 1;
    }
};

IMPL_TRAIT_FOR_CLASS(TraitD, Test3) {
    TRAIT_FOR_CLASS_SELF;

    void test() override {
        test2(test4());
    }
    void test2(int a) override {
        std::cout<<"traitD testint "<<a<<std::endl;
    }
};


struct Odd {
    virtual int number() = 0;
};

struct Even {
    virtual int number() = 0;
};

struct Number {
    virtual int number() = 0;

    int add(trait::TraitRef<Number> b) {
        return number() + b->number();
    }
};

IMPL_TRAIT_FOR_TRAIT(Number, Odd) {
    TRAIT_FOR_TRAIT_SELF(Number);
    int number() override {
        return trait::to_trait<Odd>(self).number();
    }
};

IMPL_TRAIT_FOR_TRAIT(Number, Even) {
    TRAIT_FOR_TRAIT_SELF(Number);
    int number() override {
        return trait::to_trait<Even>(self).number();
    }
};

template<class Type>
struct ZFCInt {
};

template<>
struct ZFCInt<void> {
    static constexpr int value = 0;
};

template<class T>
struct ZFCInt<ZFCInt<T>> {
    static constexpr int value = ZFCInt<T>::value + 1;
};

template<int value>
struct ZFCIntGen {
    static_assert(value > 0, "negative value not allowed");
    using IntType = ZFCInt<typename ZFCIntGen<value - 1>::IntType>;
};

template<>
struct ZFCIntGen<0> {
    using IntType = ZFCInt<void>;
};

IMPL_TRAIT_FOR_CLASS(Even, ZFCInt<void>) {
    TRAIT_FOR_CLASS_SELF;
    constexpr static int __number = 0;
    int number() override {
        return 0;
    }
};

IMPL_TRAIT_FOR_GENERIC(class Inner, Even, ZFCInt<Inner>, TRAIT_BOUND(Inner, Odd)) {
    TRAIT_FOR_GENERIC_SELF(Even, ZFCInt<Inner>);
    constexpr static int __number = trait::is_trait_h<Inner, Odd>::TraitImpl::__number + 1;
    static_assert(__number % 2 == 0, "not even");
    int number() override {
        return __number;
    }
};

IMPL_TRAIT_FOR_GENERIC(class Inner, Odd, ZFCInt<Inner>, TRAIT_BOUND(Inner, Even)) {
    TRAIT_FOR_GENERIC_SELF(Odd, ZFCInt<Inner>);
    constexpr static int __number = trait::is_trait_h<Inner, Even>::TraitImpl::__number + 1;
    static_assert(__number % 2 == 1, "not odd");
    int number() override {
        return __number;
    }
};


template<int N>
struct Integer {
};

struct NextPrim {
};

struct Prim {
};

struct NotPrim {
};

template<int Start>
struct MinPrimFactor {
};

IMPL_TRAIT_FOR_GENERIC(int N, NotPrim, Integer<N>,\
        TRAIT_PARA(std::integral_constant<bool, !trait::is_trait_h<Integer<N>, Prim>::value>)) {
};

IMPL_TRAIT_FOR_GENERIC(int N, NextPrim, Integer<N>,\
        TRAIT_BOUND(Integer<N + 1>, Prim)) {
    constexpr static int value = N + 1;
};

IMPL_TRAIT_FOR_GENERIC(int N, NextPrim, Integer<N>,\
        TRAIT_BOUND(Integer<N + 1>, NotPrim)) {
    constexpr static int value = trait::is_trait_h<Integer<N + 1>, NextPrim>::TraitImpl::value;
};

IMPL_TRAIT_FOR_GENERIC(int N, Prim, Integer<N>,\
        TRAIT_BOUND(Integer<N>, MinPrimFactor<2>),
        TRAIT_PARA(std::integral_constant<bool, trait::is_trait_h<Integer<N>, MinPrimFactor<2>>::TraitImpl::value == N>)) {
};

IMPL_TRAIT_FOR_GENERIC(TRAIT_PARA(int N, int Start), MinPrimFactor<Start>, Integer<N>,\
        TRAIT_PARA(std::integral_constant<bool, (Start > N || N % Start == 0 || Start * Start >= N)>)) {
    constexpr static int value = (N % Start == 0 ? Start : N);
};


IMPL_TRAIT_FOR_GENERIC(TRAIT_PARA(int N, int Start), MinPrimFactor<Start>, Integer<N>,\
        TRAIT_PARA(std::integral_constant<bool, !(Start > N || N % Start == 0 || Start * Start >= N)>)) {
    constexpr static int value = trait::is_trait_h<Integer<N>,
                                                    MinPrimFactor<trait::is_trait_h<Integer<Start>, NextPrim>::TraitImpl::value>
                                                   >::TraitImpl::value;
};


int main() {
    std::cout<<sizeof(trait::TraitRef<TraitA>)<<std::endl;
    std::cout<<trait::is_trait<testa::Test, TraitB><<std::endl;
    std::cout<<trait::is_trait<testa::Test, TraitA><<std::endl;
    std::cout<<trait::is_trait<testa::Test, TraitB, TraitA><<std::endl;
    auto t = testa::Test();
    (t->*(&TraitA::test))();
    (t->*(&TraitA::test2))(1);
    (t->*(&TraitB::test3))();
    {
        trait::TraitRef<TraitA> ta(t);
        ta->test();
        ta->test2(1);
        ta.cast<testa::Test>().test_call();
        (ta->*(&TraitB::test3))();
        trait::to_trait<TraitB>(ta).test3();
        trait::TraitRef<TraitB> tb = ta;
        tb->test3();
    }
    auto ta2 = trait::own<TraitA>(testa::Test());
    ta2->test();
    ta2->test2(2);
    ta2.reset();
    ta2 = trait::make<TraitA, testa::Test>();
    ta2->test();
    ta2->test2(2);
    ta2.reset();
    trait::to_trait<TraitC>(testa::Test2()).clone();
    trait::cast<testa::Test2>(trait::to_trait<TraitC>(testa::Test2())).test();
    trait::to_trait<TraitB>(testa::Test2()).test3();
    (testa::Test2()->*(&TraitB::test3))();
    trait::to_trait<TraitD>(Test3()).test();
    trait::to_trait<TraitA>(Test3()).test();
    trait::to_trait<TraitB>(Test3()).test3();
    {
        auto t3 = Test3();
        trait::TraitRef<TraitD> td(t3);
        std::cerr<<"td->test()"<<std::endl;
        td->test();
        trait::TraitRef<TraitA> ta2 = t3;
        std::cerr<<"ta2->test()"<<std::endl;
        ta2->test();
        trait::TraitRef<TraitA> ta = td;
        std::cerr<<"ta->test()"<<std::endl;
        ta->test();
        trait::TraitRef<TraitB> tb = td;
        std::cerr<<"tb->test3()"<<std::endl;
        tb->test3();
        std::cerr<<"ta->test()"<<std::endl;
        ta->test();
        auto t3p = trait::make<TraitD, Test3>();
        trait::TraitRef<TraitA> ta3 = t3p;
        ta3->test();
        trait::TraitRef<TraitB> tb3 = t3p;
        tb3->test3();
    }
    std::cout<<trait::is_trait<ZFCIntGen<12>::IntType, Odd><<std::endl;
    std::cout<<trait::is_trait<ZFCIntGen<13>::IntType, Odd><<std::endl;
    std::cout<<trait::is_trait<ZFCIntGen<14>::IntType, Even><<std::endl;
    std::cout<<trait::to_trait<Number>(ZFCIntGen<12>::IntType()).add(ZFCIntGen<11>::IntType())<<std::endl;
    return 0;
    std::cout<<"prim"<<std::endl;
    std::cout<<trait::is_trait<Integer<2>, Prim><<std::endl;
    std::cout<<trait::is_trait<Integer<3>, Prim><<std::endl;
    std::cout<<trait::is_trait<Integer<17>, Prim><<std::endl;
    std::cout<<trait::is_trait<Integer<18>, Prim><<std::endl;
    std::cout<<trait::is_trait_h<Integer<18>, NextPrim>::TraitImpl::value<<std::endl;
    std::cout<<trait::is_trait_h<Integer<18>, MinPrimFactor<2>>::TraitImpl::value<<std::endl;
    std::cout<<trait::is_trait_h<Integer<87>, MinPrimFactor<2>>::TraitImpl::value<<std::endl;
    return 0;
}
