# JNI (Java Native Interface)

> **JNI is not IPC.** There is no binder, no parcel, no second process. JNI is a **bridge inside one process**: the Java code and the C/C++ code live in the same process, share the same memory and the same threads. A JNI call is a function call, not a transaction.
>
> Compare with [AIDL](./aidl.md), which is the opposite case: two processes, and every call is marshalled through the binder driver.

In AOSP, JNI is used when the work must be done in native code, either because the code already exists in C++ or because the Java layer is too expensive. The input pipeline is one example: `InputManagerService` is a thin Java layer over the native `InputReader` and `InputDispatcher`, bridged by JNI for performance reasons.

## 0. Quick reference

| Item | Value |
| --- | --- |
| Load the library | `System.loadLibrary("foo")` loads `libfoo.so` |
| Method name in C++ | `Java_<package>_<Class>_<method>` |
| Per-thread handle | `JNIEnv*` |
| Per-process handle | `JavaVM*` |
| Entry point | `jint JNI_OnLoad(JavaVM* vm, void*)` |
| AOSP JNI sources | `frameworks/base/core/jni/`, `frameworks/base/services/core/jni/` |

Type signature letters:

| Java type | Letter | Java type | Letter |
| --- | --- | --- | --- |
| `boolean` | `Z` | `long` | `J` |
| `byte` | `B` | `float` | `F` |
| `char` | `C` | `double` | `D` |
| `short` | `S` | `void` | `V` |
| `int` | `I` | `Object` | `L<full/class/name>;` |
| array | `[` prefix | `String` | `Ljava/lang/String;` |

```text
(<parameter types>)<return type>

()V                          void method()
(I)Z                         boolean method(int)
(Ljava/lang/String;I)V       void method(String, int)
([IJ)Ljava/lang/String;      String method(int[], long)
```
> Get the signature without guessing: `javap -s -p MyClass.class`.

## 1. Java Side
```java
public class MyClass {

    static {
        System.loadLibrary("mylib");     // loads libmylib.so
    }

    // Declared, not implemented. The body lives in C++.
    private native void nativeSend(String message);

    private native long nativeInit();

    // Called from C++.
    private void onNativeCallback(String message) {
        Log.d(TAG, message);
    }
}
```

| Call | Effect |
| --- | --- |
| `System.loadLibrary("mylib")` | Finds `libmylib.so` in the app or system library path. The usual form. |
| `System.load("/path/libmylib.so")` | Loads an absolute path. |

A `native` method has no body in Java. Calling it jumps straight into the shared library. If the C++ function cannot be found, the call throws `UnsatisfiedLinkError` at the **first call**, not at load time.

## 2. Name Convention (how a Java method finds its C++ function)

This is the part that must be exact. The runtime looks for a symbol built from the fully qualified Java name:

```text
Java_<package>_<Class>_<method>
```

with the dots of the package replaced by underscores.

```java
package com.example.app;

class MyClass {
    private native void nativeSend(String message);
}
```

```cpp
extern "C"
JNIEXPORT void JNICALL
Java_com_example_app_MyClass_nativeSend(JNIEnv* env, jobject thiz, jstring message) {
    // ...
}
```

| Part | Meaning |
| --- | --- |
| `extern "C"` | **Required in C++.** Without it the C++ compiler mangles the symbol name and the runtime cannot find it. |
| `JNIEXPORT` | Marks the symbol as exported from the `.so`. |
| `JNICALL` | Calling convention. Keep it for portability. |
| `JNIEnv* env` | Always the first parameter. |
| `jobject thiz` | The Java object that made the call, the equivalent of `this`. For a `static` native method this is a `jclass` instead. |

> Generate the header instead of writing names by hand: `javac -h <out dir> MyClass.java`.

### Registering by hand (what AOSP does)
Instead of relying on the name, a library can register the mapping itself in `JNI_OnLoad`. AOSP uses this style almost everywhere.
```cpp
static const JNINativeMethod gMethods[] = {
    // { Java method name, signature, C++ function pointer }
    { "nativeSend", "(Ljava/lang/String;)V", (void*) nativeSend },
    { "nativeInit", "()J",                   (void*) nativeInit },
};

int register_com_example_app_MyClass(JNIEnv* env) {
    return jniRegisterNativeMethods(env, "com/example/app/MyClass",
                                    gMethods, NELEM(gMethods));
}
```

| | Name convention | `RegisterNatives` |
| --- | --- | --- |
| Function name | Must match exactly | Any name, it is static |
| Error reported | At first call | At load time |
| Symbol must be exported | Yes | No |
| Used by | Apps, small libraries | AOSP framework and system services |

> The class name uses **slashes**, not dots: `com/example/app/MyClass`.

## 3. The Special Handles
### `JNIEnv*` — one per thread
The interface pointer to everything JNI can do: create strings, find classes, read fields, call methods. Its table holds the addresses of all the JNI functions.
- It is passed into every native method, use that one.
- It is **valid only on the thread that received it**. Never store it in a global or pass it to another thread.

### `JavaVM*` — one per process
The process-wide handle. This one **is** safe to keep in a global, and it is how a native thread gets a `JNIEnv`.
```cpp
static JavaVM* g_vm = nullptr;

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_vm = vm;                        // keep it for later
    JNIEnv* env = nullptr;
    if (vm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    register_com_example_app_MyClass(env);
    return JNI_VERSION_1_6;           // must return the version
}
```

`JNI_OnLoad` runs once, when `System.loadLibrary` loads the `.so`.

### Summary
| Handle | Scope | Store it? |
| --- | --- | --- |
| `JNIEnv*` | one thread, one call | **No** |
| `JavaVM*` | whole process | Yes |
| `jobject`, `jclass`, `jstring` (local ref) | until the native method returns | **No**, promote to a global ref first |
| `jmethodID`, `jfieldID` | until the class is unloaded | Yes, and you should cache them |

## 4. Java calls C++

The direction that needs no extra work. Declare the method `native` in Java, implement it in C++ with the right name, and call it.
```cpp
extern "C"
JNIEXPORT void JNICALL
Java_com_example_app_MyClass_nativeSend(JNIEnv* env, jobject thiz, jstring message) {
    const char* str = env->GetStringUTFChars(message, nullptr);
    if (str == nullptr) return;        // out of memory, exception already pending
    ALOGD("message = %s", str);
    env->ReleaseStringUTFChars(message, str);
}
```

Returning a value works the same way, with the matching `j` type:
```cpp
extern "C"
JNIEXPORT jstring JNICALL
Java_com_example_app_MyClass_nativeGetName(JNIEnv* env, jobject thiz) {
    return env->NewStringUTF("hello");
}
```

| Java type | JNI type |
| --- | --- |
| `boolean`, `int`, `long`, `float` | `jboolean`, `jint`, `jlong`, `jfloat` |
| `String` | `jstring` |
| `int[]` | `jintArray` |
| `Object` | `jobject` |
| any class | `jobject`, or `jclass` for the class itself |

## 5. C++ calls Java

Three steps: **find the class -> find the method ID -> invoke it.**

```cpp
void callBackIntoJava(JNIEnv* env, jobject obj) {
    // 1. class of the object
    jclass clazz = env->GetObjectClass(obj);

    // 2. method id, from the name and the signature
    jmethodID method = env->GetMethodID(clazz, "onNativeCallback",
                                        "(Ljava/lang/String;)V");
    if (method == nullptr) {
        env->ExceptionClear();        // NoSuchMethodError is pending
        return;
    }

    // 3. call it
    jstring msg = env->NewStringUTF("from native");
    env->CallVoidMethod(obj, method, msg);
    env->DeleteLocalRef(msg);
}
```

| Need | Function |
| --- | --- |
| Class from an object | `GetObjectClass(obj)` |
| Class from a name | `FindClass("com/example/app/MyClass")` |
| Instance method | `GetMethodID` then `CallVoidMethod`, `CallIntMethod`, `CallObjectMethod` ... |
| Static method | `GetStaticMethodID` then `CallStaticVoidMethod` ... |
| Instance field | `GetFieldID` then `GetIntField` / `SetIntField` ... |
> `GetMethodID` is not free. Look it up once, in `JNI_OnLoad` or at init, and cache the `jmethodID`. Never cache across a class unload, and never cache the `jclass` without turning it into a global reference.

## 6. References and Exceptions
### References
Every `jobject` you receive or create is a **local reference**, freed automatically when the native method returns. That is fine for a short call and wrong for anything you keep.

```cpp
// keep across calls
g_callbackObj = env->NewGlobalRef(obj);
env->DeleteGlobalRef(g_callbackObj);
```

## 7. Build

AOSP, `Android.bp`:

```
cc_library_shared {
    name: "libmylib",
    srcs: ["mylib_jni.cpp"],
    shared_libs: [
        "libnativehelper",   // jniRegisterNativeMethods, jniThrowException
        "liblog",
        "libutils",
    ],
    header_libs: ["jni_headers"],
}
```

App with the NDK, `CMakeLists.txt`:

```cmake
add_library(mylib SHARED native-lib.cpp)
find_library(log-lib log)
target_link_libraries(mylib ${log-lib})
```

```
android {
    externalNativeBuild {
        cmake { path "src/main/cpp/CMakeLists.txt" }
    }
}
```

## 8. Practical Notes

- A JNI transition is cheap compared to binder, but not free. Prefer **one call carrying a lot of data** over many small calls in a loop.
- Cache `jmethodID`, `jfieldID` and global `jclass` references at init. Looking them up on every call is the most common performance mistake.
- `GetStringUTFChars` returns **modified UTF-8**, not standard UTF-8. It differs for the null character and for characters outside the basic plane.
- Every `Get*Chars` and `Get*ArrayElements` has a matching `Release*`. Missing one is a leak.
- A native crash takes the whole process down. There is no exception to catch, only a tombstone.
- Debug with `-Xcheck:jni` enabled, it turns silent misuse into a clear abort:

```bash
adb shell setprop dalvik.vm.checkjni true
adb shell stop && adb shell start
```

- Useful places to read real code:
| Path | What is there |
| --- | --- |
| `frameworks/base/core/jni/` | JNI for the framework classes |
| `frameworks/base/services/core/jni/` | JNI for the system services, including the input manager |
| `libnativehelper/` | `jniRegisterNativeMethods`, `jniThrowException`, scoped helpers |
