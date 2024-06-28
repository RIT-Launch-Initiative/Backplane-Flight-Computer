#ifndef C_TENANT_H
#define C_TENANT_H

class CTenant {
public:
    explicit CTenant(const char *name);

    virtual ~CTenant() = default;

    virtual void Startup() {};

    virtual void PostStartup() {};

    virtual void Run() = 0;
protected:
    const char *name;

};

#endif //C_TENANT_H
