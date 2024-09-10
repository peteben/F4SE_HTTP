#include <RE/Fallout.h>


class HttpEventHandler  {
	std::uint64_t varObjHandle;							// Handle of scriptVar's object (calling script)
	RE::BSFixedString varObjName;						// Name of scriptVar's object
	RE::BSSpinLock spin;

public:

	[[nodiscard]] static HttpEventHandler* GetSingleton() {
		static HttpEventHandler singleton;
		return std::addressof(singleton);
		}

	void Register(const RE::BSScript::Variable* scriptVar) {		//Script variable passed to RegisterForHttpEvent()
		const RE::BSAutoLock lock{ spin };
		if (auto GameVM = RE::GameVM::GetSingleton()) {
			auto object = RE::BSScript::UnpackVariable<RE::BSTSmartPointer<RE::BSScript::Object>>(*scriptVar);
			if (!object || object->GetHandle() == GameVM->handlePolicy.EmptyHandle() || !object->type) {
				return;
				}

			GameVM->handlePolicy.PersistHandle(object->GetHandle());
			varObjHandle = object->GetHandle();
			varObjName = object->type->GetName();
			}
		}

	template <class... Args>
	void Dispatch(std::string eventName, Args... a_args) {
		auto GameVM = RE::GameVM::GetSingleton();
		if (!GameVM) {
			return;
			}

		auto VM = GameVM->GetVM();
		if (!VM) {
			return;
			}

		const RE::BSAutoLock lock{ spin };
		VM->DispatchMethodCall(varObjHandle, varObjName, eventName.data(), nullptr, a_args...);
		}
	};

void RegisterForHttpEvent(std::monostate, const RE::BSScript::Variable* a_this) {
	HttpEventHandler::GetSingleton()->Register(a_this);
	}

void UnregisterForHttpEvent(std::monostate, const RE::BSScript::Variable* a_this) {
	//detail::HttpEventHandler::GetSingleton()->Unregister(a_this);
	}

void SendToPapyrus(int handle, std::string functionName) {
	HttpEventHandler::GetSingleton()->Dispatch(functionName, handle);
	}
