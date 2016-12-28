/*union ThunkData {
    void* function;
    void* result;
}*/
#include <functional>
#include <queue>
#include <stdlib.h>

#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>

class Thunk;

class Thunk {
private:
    void* value;
    std::queue<std::function<void*(void*)>>* suspensions;
public:
    Thunk(void* value) {this->value = value; this->suspensions = new std::queue<std::function<void*(void*)>>();}
    ~Thunk() {delete suspensions;}
    bool isTerminal() const {return suspensions->size()==0;}
    Thunk* evalAll() {
        while(!isTerminal()) {
            void* temp = suspensions->front()(value);
            suspensions->pop();
            value = temp;
        }
        return this;
    }
    void* evalGet() {
        return this->evalAll()->value;
    }

    //The enclosing value in the monad is the eventual result of the computation.
    Thunk* fmap(std::function<void*(void*)> f) {
        Thunk* temp = delay([f, this]()->void*{return f(this->evalGet());});
        return temp;
    }
    Thunk* bind(std::function<Thunk*(void*)> f) {
        Thunk* temp =  this->branch();
        temp->suspensions->push([f](void* val)->void*{return f(val);});
        temp = temp->join();
        return temp;
    }
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
    Thunk* bind_del(std::function<Thunk*(void*)> f) {
        this->suspensions->push([f](void* val)->void*{return f(val);});
        this->join_del();
        return this;
    }
    
    //Destructively joins this.
    //Similar caveat as bind_del
    Thunk* join_del() {
        this->suspensions->push([](void* val) {
            Thunk* inner = (Thunk*) val;
            return inner->evalGet();
        });
        return this;
    }
    
    //Branches this Thunk (lazily of course). When the new thunk requres a value,
    //It will use the value of this thunk if it has been evaluated, otherwise it
    //eill evaluate it on the spot.
    //Effectively a copy constructor, but refers back to the caller.
    Thunk* branch() {
        return delay([this]()->void*{return this->evalGet();});
    }
    
    
    //User-friendly operators.
    
    friend Thunk* operator >>= (Thunk* const& obj, const std::function<Thunk*(void*)>& f) {return obj->bind(f);}
    friend Thunk* operator >> (Thunk* const& obj, const std::function<Thunk*()>& f) {return obj->bind([f](void* dummy)->Thunk*{return f();});}
            
    friend Thunk* operator <<= (const std::function<Thunk*(void*)>& f, Thunk* const& obj) {return obj->bind(f);}
    friend Thunk* operator <<(const std::function<Thunk*()>& f, Thunk* const& obj) {return obj>>f;}
    //Cannon implement the below because one type in an operator must be a class
    //friend Thunk* operator >> (Thunk* obj, Thunk* next) {return this->bind(
    //    [next](void* dummy)->Thunk*{return next;});}
    //friend Thunk* operator << (Thunk* next, Thunk* const* obj) {return obj>>next;}
    
    static Thunk* pure(void* value) {
        return new Thunk(value);
    }
    
    //The main way to construct a thunk. Delay a buffered-by-nullary-value computation
    static Thunk* delay(std::function<void*()> f) {
        Thunk* temp = new Thunk(nullptr);
        temp->suspensions->push([f](void* dummy)->void*{return f();});
        return temp;
    }
    static std::function<Thunk*(Thunk*)> lift(std::function<void*(void*)> f) {
        return [f](Thunk* thunkIn)->Thunk*{
            return delay([f, thunkIn]()->void*{return f(thunkIn->evalGet());});
        };
    }
    static std::function<std::function<Thunk*(Thunk*)>(Thunk*)> lift2(std::function<std::function<void*(void*)>(void*)> f) {
        return [f](Thunk* thunkIn)->std::function<Thunk*(Thunk*)>{
            auto intermediate = f(thunkIn->evalGet());
            return lift(intermediate);
        };
    }
    
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

Thunk* addTwoMonadic(int* a) {return Thunk::delay([a]()->void*{*a+=2; return a;});}
int* addTwoUnlifted(int* a) {*a+=2; return a;}

int main()
{
    //Merely represents computations to perform on a.
    Thunk* aThunk = Thunk::delay([]()->void*{return new int(5);}); 
    //Also represents computations on a.
    auto copyInt = [](void* a)->void*{return new int(*(int*)a);};
    auto bThunk = aThunk
        ->fmap(copyInt); //Make a copy of a's value
    for(int i=0; i<100000; i++)
    {
        bThunk = bThunk->bind_del(reinterpret_cast<Thunk*(*)(void*)>(addTwoMonadic));
    }
    auto addTwoLifted = Thunk::lift(reinterpret_cast<void*(*)(void*)>(addTwoUnlifted));
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
    

    auto freeInt = [](void* toFree)->void*{delete (int*)toFree; return nullptr;};
    aThunk->fmap(freeInt)->evalAll();
    bThunk->fmap(freeInt)->evalAll(); //Copy of a, so no double deletion
    cThunk->fmap(freeInt)->evalAll();
    
    cout << "Freed underlying data." << endl;
    
    //Delete the thunks
    delete aThunk;
    delete bThunk;
    delete cThunk;
    
    cout << "Freed thunks." << endl;
}