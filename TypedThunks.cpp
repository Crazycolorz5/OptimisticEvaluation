#ifndef THUNKS
#define THUNKS

#include <functional>
#include <queue>
#include <stdlib.h>

#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>

#include "Monad.cpp"

//template <typename T>
//class Thunk;


//t->forall r.r = template <typename t, template <typename r>>
template <typename T>
class Thunk;

class Thunks {
public:
    template<typename A>
    static Thunk<A>* delay(std::function<A*()> f);
    template <typename A, typename B>
    static std::function<Thunk<B>*(Thunk<A>*)> lift(std::function<B*(A*)> f);
    template <typename A, typename B>
    static std::function<Thunk<B>*(Thunk<A>*)> lift_del(std::function<B*(A*)> f);
    template <typename A, typename B, typename C>
    static std::function<std::function<Thunk<C>*(Thunk<B>*)>(Thunk<A>*)> lift2(std::function<std::function<C*(B*)>(A*)> f);
};

template <typename T>
class Thunk : Monad<Thunk>{
private:
    void* value;
    std::queue<std::function<void*(void*)>>* suspensions;
public:
    Thunk(T* value) {/*printf("Thunk made.\n");*/this->value = value; this->suspensions = new std::queue<std::function<void*(void*)>>();}
    ~Thunk() {/*printf("Thunk deleted.\n");*/delete suspensions;}
    bool isTerminal() const {return suspensions->size()==0;}
    Thunk* evalAll() {
        while(!isTerminal()) {
            void* temp = suspensions->front()(value);
            suspensions->pop();
            value = reinterpret_cast<void*>(temp); 
        }
        return this;
    }
    T* evalGet() {
        return (T*)this->evalAll()->value;
    }

    //The enclosing value in the monad is the eventual result of the computation.
    template <typename R>
    Thunk<R>* fmap(std::function<R*(T*)> f) {
        Thunk<R>* temp = Thunks::delay<R>([f, this]()->R*{return f(this->evalGet());});
        return temp;
    }
    //template <typename A, typename B>
    //m<B>* bind(std::function<m<B>(A)>);
    template <typename R>
    Thunk* bind(std::function<Thunk<R>*(T*)> f) {
        Thunk* temp =  this->branch();
        temp->suspensions->push([f](void* val)->void*{return f((T*)val);});
        temp = temp->join();
        return temp;
    }
    
    //join:: Thunk<Thunk<T>> -> Thunk<T>
    //idk if I can say that in C++ so I'll just let sleeping dogs lie...
    Thunk* join() {
        Thunk* temp = this->branch();
        temp->suspensions->push([](void* val)->void*{
            Thunk* inner = (Thunk*) val;
            return inner->evalGet();
        });
        return temp;
    }
    
    //Destructively binds f onto this.
    //Use this if and only if your statment was going to be myThunk = myThunk->bind(f);
    //Allowed only for monomorphisms
    Thunk<T>* bind_del(std::function<Thunk<T>*(T*)> f) {
        this->suspensions->push([f](void* val)->void*{return f((T*)val);});
        return this->join_del();
    }
    
    //Destructively joins this.
    //Similar caveat as bind_del
    Thunk* join_del() {
        this->suspensions->push([](void* val) {
            Thunk* inner = (Thunk*) val;
            void* innerVal = inner->evalGet();
            delete inner;
            return innerVal;
        });
        return this;
    }
    
    template <typename R>
    Thunk<R>* fmap_del(std::function<R*(T*)> f) {
        return Thunks::delay<R>([f, this]()->R*{R* temp = f(this->evalGet()); delete this; return temp;});
    }
    
    
    //Branches this Thunk (lazily of course). When the new thunk requres a value,
    //It will use the value of this thunk if it has been evaluated, otherwise it
    //eill evaluate it on the spot.
    //Effectively a copy constructor, but refers back to the caller.
    Thunk* branch() {
        return Thunks::delay<T>([this]()->T*{return this->evalGet();});
    }
    
    
    //User-friendly operators.
    //TODO: Maybe use bind_del?
    friend Thunk* operator >>= (Thunk* const& obj, const std::function<Thunk*(void*)>& f) {return obj->bind(f);}
    friend Thunk* operator >> (Thunk* const& obj, const std::function<Thunk*()>& f) {return obj->bind([f](void* dummy)->Thunk*{return f();});}
            
    friend Thunk* operator <<= (const std::function<Thunk*(void*)>& f, Thunk* const& obj) {return obj->bind(f);}
    friend Thunk* operator <<(const std::function<Thunk*()>& f, Thunk* const& obj) {return obj>>f;}
    //Cannon implement the below because one type in an operator must be a class
    //friend Thunk* operator >> (Thunk* obj, Thunk* next) {return this->bind(
    //    [next](void* dummy)->Thunk*{return next;});}
    //friend Thunk* operator << (Thunk* next, Thunk* const* obj) {return obj>>next;}
    
    
    //static m<A>* pure(A*);
    template <typename A>
    static Thunk<A>* pure(A* value) {
        return new Thunk<A>(value);
    }
    
    //The main way to construct a thunk. Delay a buffered-by-nullary-value computation
    //template <typename T>
    template <typename A>
    friend Thunk<A>* Thunks::delay(std::function<A*()> f);
    template <typename A, typename B>
    friend std::function<Thunk*(Thunk*)> Thunks::lift(std::function<B*(A*)> f);
    template <typename A, typename B>
    friend std::function<Thunk*(Thunk*)> Thunks::lift_del(std::function<B*(A*)> f);
    template <typename A, typename B, typename C>
    friend std::function<std::function<Thunk<C>*(Thunk<B>*)>(Thunk<A>*)> Thunks::lift2(std::function<std::function<C*(B*)>(A*)> f);
    
    //For printing
    std::string toString() const { 
        std::ostringstream ss;
        if(this->isTerminal()) {
            ss << value;
        } else {
            ss << '_';
        }
        return ss.str();
    }
};

template <typename A>
Thunk<A>* Thunks::delay(std::function<A*()> f) {
        Thunk<A>* temp = new Thunk<A>(nullptr);
        temp->suspensions->push([f](void* dummy)->void*{return f();});
        return temp;
}
template <typename A, typename B>
std::function<Thunk<B>*(Thunk<A>*)> Thunks::lift(std::function<B*(A*)> f) {
    return [f](Thunk<A>* thunkIn)->Thunk<B>*{
        return Thunks::delay<B>([f, thunkIn]()->B*{return f(thunkIn->evalGet());});
    };
}
template <typename A, typename B>
std::function<Thunk<B>*(Thunk<A>*)> Thunks::lift_del(std::function<B*(A*)> f) {
    return [f](Thunk<A>* thunkIn)->Thunk<B>*{
        return Thunks::delay<B>([f, thunkIn]()->B*{B* temp = f(thunkIn->evalGet()); delete thunkIn; return temp;});
    };
}
template <typename A, typename B, typename C>
std::function<std::function<Thunk<C>*(Thunk<B>*)>(Thunk<A>*)> Thunks::lift2(std::function<std::function<C*(B*)>(A*)> f) {
    return [f](Thunk<A>* thunkIn)->std::function<Thunk<C>*(Thunk<B>*)>{
        auto intermediate = f(thunkIn->evalGet());
        return Thunks::lift<B, C>(intermediate);
    };
}



Thunk<int>* addTwoMonadic(int* a) {return Thunks::delay<int>([a]()->int*{*a+=2; return a;});}
int* addTwoUnlifted(int* a) {*a+=2; return a;}

/*
int main()
{
    //Merely represents computations to perform on a.
    Thunk<int>* aThunk = Thunks::delay<int>([]()->int*{return new int(5);}); 
    //Also represents computations on a.
    auto copyIntToDouble = [](int* a)->double*{return new double(*a);};
    auto doubleToInt = [](double* a)->int*{int* temp = new int(*a); delete a; return temp;};
    aThunk = aThunk->bind_del(addTwoMonadic);
    auto bThunk = aThunk->branch(); //Make a copy of a (no aliasing)
    auto dThunk = bThunk->fmap_del<double>(copyIntToDouble); //Make a copy of a's value (no deep aliasing)
    bThunk = dThunk->fmap_del<int>(doubleToInt);
    for(int i=0; i<10000; i++)
    {
        bThunk->bind_del(addTwoMonadic);
    }
    auto addTwoLifted = Thunks::lift_del<int, int>(addTwoUnlifted);
    auto cThunk = 
        addTwoLifted(
        addTwoLifted(
        addTwoLifted(
            addTwoMonadic(new int(10))
                )));
    //I need my monad IO system to print stuff here...
    
    
    //Now set the thunk deallocators.
    //Actually do this below the IO because lol no composable IO functions
    
    using namespace std;
    cout << "Evaluating bThunk first test:" << endl << endl;
    
    
    cout << "aThunk: " << aThunk->toString() << endl; 
    cout << "bThunk: " << bThunk->toString() << endl; 
    cout << "cThunk: " << cThunk->toString() << endl << endl; 
    
    printf("bThunk=>%d\n", *(int*)bThunk->evalGet());
    
    cout << "aThunk: " << aThunk->toString() << endl; 
    cout << "bThunk: " << bThunk->toString() << endl; 
    cout << "cThunk: " << cThunk->toString() << endl << endl; 
    
    printf("aThunk=>%d\n", *(int*)aThunk->evalGet());

    cout << "aThunk: " << aThunk->toString() << endl; 
    cout << "bThunk: " << bThunk->toString() << endl; 
    cout << "cThunk: " << cThunk->toString() << endl << endl; 
    
    printf("cThunk=>%d\n", *(int*)cThunk->evalGet());

    cout << "aThunk: " << aThunk->toString() << endl; 
    cout << "bThunk: " << bThunk->toString() << endl; 
    cout << "cThunk: " << cThunk->toString() << endl << endl; 
    

    auto freeInt = [](int* toFree)->void*{delete (int*)toFree; return nullptr;};
    delete aThunk->fmap<void>(freeInt)->evalAll(); //Make the thunks for immediate use
    delete bThunk->fmap<void>(freeInt)->evalAll(); //Copy of a, so no double deletion
    delete cThunk->fmap<void>(freeInt)->evalAll();
    
    cout << "Freed underlying data." << endl;
    
    //Delete the thunks
    delete aThunk;
    delete bThunk;
    delete cThunk;
    
    cout << "Freed thunks." << endl;
}*/

#endif