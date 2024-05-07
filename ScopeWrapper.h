template <typename Type>
class ScopeWrapper {
    void (*_exit_func)(Type);
    bool _is_valid;
    Type _state;

public:
    ScopeWrapper(Type (*enter_func)(), void (*exit_func)(Type)) {
        _exit_func = exit_func;
        _state = enter_func();
        validate();
    }

    ~ScopeWrapper() { _exit_func(_state); }

    [[nodiscard]]
    bool is_valid() const {
        return _is_valid;
    }

    void validate() {
        _is_valid = true;
    }

    void invalidate() {
        _is_valid = false;
    }
};

#define WRAP_SCOPE(type, entr, exit) for(ScopeWrapper<type> _sw(entr, exit); _sw.is_valid(); _sw.invalidate())
