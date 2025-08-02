#pragma once

#include "../../Core/Utilities/Types.h"
#include "../RHI/IRHIContext.h"

class IBindable {
public:
    virtual ~IBindable() = default;
    
    virtual void Bind(IRHIContext& context) = 0;
    virtual void Unbind(IRHIContext& context) {}
    
    virtual bool IsValid() const = 0;
    virtual const String& GetDebugName() const = 0;
    
protected:
    String m_debugName = "Unnamed Bindable";
};