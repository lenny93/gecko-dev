#ifndef __SPECIALTHING_IMPL_H__
#define __SPECIALTHING_IMPL_H__

#include "nsIEditorGrammarCheck.h"

#define NS_GRAMMARCHECK_CONTRACTID "@mozilla.org/grammarcheck;1"
#define NS_GRAMMARCHECK_CLASSNAME "nsEditorGrammarCheck"
#define NS_GRAMMARCHECK_CID { 0x8ac26150, 0x586f, 0x4b70, \
  { 0x90, 0xdf, 0x07, 0x61, 0x0b, 0x80, 0xb4, 0x5f } }
  
#define GC_FACTORY_SINGLETON_IMPLEMENTATION(_className, _sInstance)        \
_className * _className::_sInstance = nullptr;                                \
																		   \
already_AddRefed<_className>                                                 \
_className::GetSingleton()                                                   \
{                                                                            \
if (_sInstance) {                                                          \
  nsRefPtr<_className> ret = _sInstance;                                   \
  return ret.forget();                                                     \
}                                                                          \
_sInstance = new _className();                                             \
nsRefPtr<_className> ret = _sInstance;                                     \
if (NS_FAILED(_sInstance->Init())) {                                       \
  /* Null out ret before _sInstance so the destructor doesn't assert */    \
  ret = nullptr;                                                           \
  _sInstance = nullptr;                                                    \
  return nullptr;                                                          \
}                                                                          \
return ret.forget();                                                       \
}

class nsEditorGrammarCheck final : public nsIEditorGrammarCheck
{
public:
	nsEditorGrammarCheck();
	
	static already_AddRefed<nsEditorGrammarCheck> GetSingleton();
	
	static nsEditorGrammarCheck* GetGrammarCheckService()
	{
		if (!gGrammarCheckService) 
		{
			nsCOMPtr<nsIEditorGrammarCheck> serv = do_GetService(NS_GRAMMARCHECK_CONTRACTID);
			NS_ENSURE_TRUE(serv, nullptr);
			NS_ASSERTION(gGrammarCheckService, "Should have static instance pointer now");
		}
		return gGrammarCheckService;
	}
	
	nsresult Init();

    /**
     * This macro expands into a declaration of the nsISupports interface.
     * Every XPCOM component needs to implement nsISupports, as it acts
     * as the gateway to other interfaces this component implements.  You
     * could manually declare QueryInterface, AddRef, and Release instead
     * of using this macro, but why?
     */
    // nsISupports interface
    NS_DECL_ISUPPORTS

    /**
     * This macro is defined in the nsISample.h file, and is generated
     * automatically by the xpidl compiler.  It expands to
     * declarations of all of the methods required to implement the
     * interface.  xpidl will generate a NS_DECL_[INTERFACENAME] macro
     * for each interface that it processes.
     *
     * The methods of nsISample are discussed individually below, but
     * commented out (because this macro already defines them.)
     */
    NS_DECL_NSIEDITORGRAMMARCHECK
	
private:
	~nsEditorGrammarCheck();
	static nsEditorGrammarCheck* gGrammarCheckService;
	nsIEditorGrammarCheckCallback* gCallback;
};

#endif