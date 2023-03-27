# rust_trait
Demo rust-like trait implementation with C++14

Use Google translate / ChatGPT to get your English version

## 摘要

使用C++14实现了类似Rust中的trait的机制。Rust没有C++的基于继承的多态机制，相应地使用一种称为trait的机制：

用户可以声明一个trait，类似于一种接口。接下来，用户可以声明为某种类型实现这个trait，从而可以对这个类型使用
trait中声明的方法。trait接口的使用既可以静态分发，也可以动态分发。

Rust还可以针对泛型定义trait实现，泛型可以定义trait bound，表示约束对应类型必须实现了相应的trait。
trait bound可以指定多个trait。trait和类型本身也可以支持泛型，可以对泛型参数进行trait bound约束。

rust_trait.h中使用C++14实现了类似的机制，用户可以声明一个struct/class作为一个trait，然后使用定制的宏针对
某种类型定义一个trait实现。编译时多态通过C++ template实现，而运行时多态通过在trait类中定义虚函数实现。

## 代码说明

全部实现包含在rust_trait.h中。test.cpp是一个用于验证的C++可执行程序，可以通过gcc或clang编译：
```
g++ -std=c++14 -O2 -o test test.cpp
```
验证过gcc 5和clang 8 for linux，可以正确编译运行。clang 3.8对编译时类型递归的处理似乎有一些问题，导致比较复杂的trait
声明和实现编译时超出递归层级限制或编译器崩溃。具体编译器版本相关的已知问题会在后面列出。

## 用法

### Trait声明与实现

```C++
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
```
上面的代码中定义了一个trait `TraitA`，它是个只包含纯虚函数的接口类。使用接口类并非强制的要求，接口类也可以没有虚函数，
可以包含静态方法，可以是模板类型，虚函数可以有默认实现（其中可以调用其他虚函数）。但一般希望接口类不包含实际的数据成员，
这样支持最好，也最符合trait的思想。

`testa::Test`是我们希望实现这个trait的类型，它也只是个普通的类，没有任何特别的要求。

全部的实现的部分在于下面的宏`IMPL_TRAIT_FOR_CLASS`，它会定义一个新的trait实现，并关联trait类型和基础类型。这个
新的实现是`TraitA`的一个子类，必须实现`TraitA`的所有纯虚函数。通过TRAIT_FOR_CLASS_SELF宏会自动定义`Self`类型和`self`成员变量，
其中`Self`是希望实现trait的类型的别名，这里也就是`testa::Test`；`self`是被转换为trait的对象的引用。

为内置类型和其它库中定义的类型实现trait也没有任何问题：

```C++
IMPL_TRAIT_FOR_CLASS(TraitA, int) {
    TRAIT_FOR_CLASS_SELF;
    void test() override {
        std::cout<<"test for int"<<std::endl;
    }
    void test2(int a) override {
        std::cout<<"testint for int "<<a<<std::endl;
    }
};

IMPL_TRAIT_FOR_CLASS(TraitA, std::unique_ptr<int>) {
    TRAIT_FOR_CLASS_SELF;
    void test() override {
        std::cout<<"test for unique ptr int"<<std::endl;
    }
    void test2(int a) override {
        std::cout<<"testint for unique ptr int "<<a<<std::endl;
    }
};

IMPL_TRAIT_FOR_CLASS(TraitA, std::unique_ptr<float>) {
    TRAIT_FOR_CLASS_SELF;
    void test() override {
        std::cout<<"test for unique ptr float"<<std::endl;
    }
    void test2(int a) override {
        std::cout<<"testint for unique ptr float "<<a<<std::endl;
    }
};
```

可以为泛型实现trait，其中一种比较简单的形式是为实现了某些trait的类型实现另一个trait：
```C++
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
```
使用`IMPL_TRAIT_FOR_TRAIT`时，需要配合使用`TRAIT_FOR_TRAIT_SELF`，会自动引入泛型类型`Self`和相应的引用`self`。
基于C++ template的功能，这里的self可以访问实际类型中的所有成员，不过考虑到通用性，转换成约束过的trait类型然后
调用相应的方法是一个通用的办法。

IMPL_TRAIT_FOR_TRAIT的第一个参数是需要实现的trait，第二个开始的参数代表trait bound约束，可以指定多个，指定多个
时只有相应类型实现了所有指定的trait bound的时候才会应用。

也可以更为通用地使用泛型（template）：
```C++
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
```
使用`IMPL_TRAIT_FOR_GENERIC`可以自行定制template模板参数，它的第一个参数是模板参数列表（含有逗号时，可以用`TRAIT_PARA`
宏括起来防止宏解析出现问题），第二个参数是希望实现的trait，第三个参数是希望实现这个trait的原始类型，第四个开始的
参数代表泛型约束，其中可以通过TRAIT_BOUND指定trait bound约束，也可以通过任意`std::integral_constant<bool>`类型，即
含有bool类型的value成员的类型来指定通用约束，比如std::is_same_type、std::is_base_of等。只有所有约束都符合时才会对目标
类型应用这个实现的版本。注意template模板参数必须可以从第二个和第三个参数，也就是trait和原始类型中推断出来。

`IMPL_TRAIT_FOR_TRAIT`实质上相当于`IMPL_TRAIT_FOR_GENERIC(class Self, Trait, Self, TRAIT_BOUND(Self, TraitBound1, TraitBound2, ...))`

trait类型本身可以包含泛型参数，参数的设计是任意的，但其中一种较为常见的形式是将未来的实际类型`Self`作为模板参数，用于在参数和返回值中使用和实际类型相同或相关的类型：
```C++
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
```
注意到`IMPL_TRAIT_FOR_TRAIT`中`Self`可以被用于指定trait和trait bound，因为它是隐式引入的模板参数。使用`IMPL_TRAIT_FOR_CLASS`时则需要显式指定类型。

trait和原始类型可以和其它类型一样使用继承，其中原始类型的继承没有限制，可以使用多继承、虚继承等，如果基类实现了trait，子类自动视为具有相应的trait实现；
trait可以继承其他trait，如果为某个类型实现了trait子类型，则视为父类型也同时被实现。使用`TraitRef`的情况下，有一些额外的限制，可以参考后面的内容。

```C++
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
```

### 使用trait方法

C++不支持像rust一样直接从原始类型访问trait方法，基本的用法是通过`trait::to_trait`将类型转化为对应的trait对象然后调用相应的方法：

```C++
trait::to_trait<TraitA>(1).test();
```

`to_trait`方法需要指定trait作为模板参数，接受一个左值或者右值引用，它会自动查找这个类型是否实现了相应的trait（包括直接针对类型
实现和通过泛型 + trait bound间接实现），并且返回**实际实现类的实例**。这意味着使用取得的对象（以及对象的类型）不仅可以访问基类
中声明的虚函数，也可以直接访问实现类当中新定义的方法、静态方法、类型、常量、子类等。`to_trait`对一些类型有特化，后面会详细介绍，
整体上来说它保证对于所有参数都会返回能转化为trait的子类实例或者trait的引用，因而保证都可以调用trait中声明的方法。

rust_trait.h中重载了->\*操作符，使得任意类型可以通过trait的指向类成员的指针访问trait方法，在级联调用的时候比`to_trait`略微可读一些：

```C++
auto t = testa::Test();
(t->*(&TraitA::test))();
(t->*(&TraitA::test2))(1);
(t->*(&TraitB::test3))();
```
### trait的动态分发

和其他C++类型一样，可以传递trait对象的引用给其他函数，并通过引用访问trait方法，这将使用虚函数表，产生虚函数调用。除此以外，也可以使用`TraitRef`和`TraitUPtr`来保存一个trait并且擦除类型，它们的区别是`TraitRef`仅保存原始实例的引用，而`TraitUPtr`会持有原始类型实例，析构时同时将原始实例析构。后两种类型不仅适合用于函数和方法的参数类型，也可以用于容器类型。

可以使用`TraitRef`类型接收一个trait：

```C++
struct Number {
    virtual int number() = 0;

    int add(trait::TraitRef<Number> b) {
        return number() + b->number();
    }
};
```
和泛型 + trait bound相比，这种情况下类型是擦除的，不会引发代码膨胀，但调用开销略有增加。和直接使用trait引用相比，在64位系统上，大部分trait类型的`TraitRef`类型为16字节，
其中包含了完整的trait对象的信息，只要原始类型实例的引用仍然有效，`TraitRef`就有效，无需关心中间产生的临时trait对象的生命周期，可以直接将对应参数声明为值类型。

> **Warning**
> `TraitRef`的实现使用了一部分被标准认为是UB的特性，但在符合条件的情况下，主流编译器上都可以使用。具体来说，
> `TraitRef`对象的移动、复制（包括作为参数传递给其他函数）假定trait对象被编译为一个虚表指针 + self对象的引用，并且这两部分都可以直接内存复制，这不符合
> C++标准，但在主流编译器上成立。同时，`TraitRef`假定trait基类的指针和trait实现的指针指向同一位置，这一点并未被C++标准保证，但主流编译器在单继承的情况下
> 符合。`TraitRef`通过将实际trait实现类inplace构造到对象内部，然后将起始指针reinterpret_cast为基类指针的方式工作，这种方式确保各类开销最小化，但不
> 完全遵循C++标准。配合trait继承使用时，要求trait子类指针和基类指针没有偏移，实现类大小也要一致，因此只能用于单继承且子类中没有额外数据成员的情况。
>
> `TraitRef`接收`TraitUPtr`类型，或者将`TraitUPtr`转化为`TraitRef`时适用`TraitRef`复制类似的机制，有相似的要求。

`TraitUPtr`实际上是`unique_ptr`的别名，通过`trait::own`或`trait::make`的方式构造，`own`接受对象右值或对象的`unique_ptr`右值，使用合适的子类保存对象。
`make`会直接构造基础类型形成智能指针，类似`make_unique`。**使用`own`的情况下，如果原始类型和原始类型的智能指针都实现了相应的trait，优先认为是使用智能
指针指向的对象。**

`TraitRef`和`TraitUPtr`类似于指针类型，需要使用->或者*解引用来访问trait方法，直接使用.使用的是`TraitRef`/`TraitUPtr`类型本身的成员方法。

### 特殊规则

对特殊类型适用以下规则：
1. trait的实现类的实例视为实现了相应的trait，使用`to_trait`会复制或移动构造一个新的实例
2. TraitRef视为实现了相应的trait，使用`to_trait`会将`TraitRef`解引用返回相应trait的引用
3. TraitUPtr视为实现了相应的trait，使用`to_trait`会将`TraitUPtr`解引用返回相应trait的引用

这些规则保证使用to_trait等方法时不会出现过于意外的情况。

### 反向转换

在知道原始类型的情况下，可以从trait基类引用、`TraitRef`或`TraitUPtr`取回原始类型的引用，需要使用`trait::cast`方法。
使用`trait::cast`有以下情况：

1. 指定和原始类型匹配的类型，传入trait引用，取得正确的实例引用；类型不匹配时为UB
2. 指定和原式类型匹配的类型，传入trait实际实现实例的引用，取得正确的实例引用；类型不匹配时会编译失败。
3. 指定`TraitRef`类型，传入trait引用，获得TraitRef类型的引用，这是为了适应前面规定`TraitRef`视为实现了trait的规则

### 类型检查

可以通过`trait::is_trait<Base, Trait...>`模板常量检查指定类型是否实现了指定的所有trait。`trait::is_trait_h`有类似
的作用，它的`value`静态成员是`is_trait`对应的值，如果只指定一个trait，且存在相应的实现类型，则`TraitImpl`成员是
相应的实现类型的别名，可以用于访问静态成员。

`trait_assert`可以用于SFINAE，它实际上是`std::enable_if_t`和`is_trait`组合使用的结果，存在相应实现类型时，返回原始类型，
否则无法正确编译，从而触发SFINAE来防止生成相应的模板实例。

## 原理

（待续）

## 限制

（待续）

## 已知的编译器bug的影响

（待续）

## 缺点和进一步可能的改进

（待续）
