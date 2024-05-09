template <typename Type>
class ScopeWrapper {
    void (*_exit_func)(Type);
    Type _state;

public:
    ScopeWrapper(Type (*enter_func)(), void (*exit_func)(Type)) {
        _exit_func = exit_func;
        _state = enter_func();
    }

    ~ScopeWrapper() { _exit_func(_state); }
};

#define WRAP_SCOPE(type, entr, exit) for(struct {ScopeWrapper<type> sw; bool flag;} _scope = {ScopeWrapper<type>(entr, exit), true}; _scope.flag; _scope.flag = false)
