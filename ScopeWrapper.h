
class ScopeWrapper {
    void (*_exit_func)();
    bool _is_valid;

public:
    ScopeWrapper(void (*enter_func)(), void (*exit_func)()) {
        _exit_func = exit_func;
        enter_func();
        _is_valid = true;
    }

    ~ScopeWrapper() { _exit_func(); }

    [[nodiscard]]
    bool is_valid() const {
        return _is_valid;
    }

    void invalidate() {
        _is_valid = false;
    }
};

#define WRAP_SCOPE(entr, exit) for(ScopeWrapper _sw(entr, exit); _sw.is_valid(); _sw.invalidate())
