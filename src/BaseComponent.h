#pragma once

#define DEFAULT_DESTRUCTOR_COMPONENT(CLASS_NAME) \
	virtual ~CLASS_NAME() \
	{\
		if (IsDestroyed())\
		{\
			return;\
		}\
		CLASS_NAME::Destroy();\
	}


class BaseComponent
{
public:
	virtual void Destroy() {}
	// virtual ~BaseComponent();
	const bool& IsDestroyed() const
	{
		return isDestroyed;
	}
	DEFAULT_DESTRUCTOR_COMPONENT(BaseComponent)
protected:
	BaseComponent() = default;
private:
	bool isDestroyed = false;
};

