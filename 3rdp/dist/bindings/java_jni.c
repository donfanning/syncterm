
#include "../crypt.h"

#ifdef USE_JAVA

#include <jni.h>
#include <stdio.h>  //printf
#include <stdlib.h> //malloc, free



/* Helper Functions */

int processStatus(JNIEnv *env, jint status)
{
    jclass exClass;
    jmethodID exConstructor;
    jthrowable obj;

    if (status >= cryptlib_crypt_OK)
        return 1;

    exClass = (*env)->FindClass(env, "cryptlib/CryptException");
    if (exClass == 0)
    {
        printf("java_jni.c:processStatus - no class?!\n");
        return 0;
    }

    exConstructor = (*env)->GetMethodID(env, exClass, "<init>", "(I)V");
    if (exConstructor == 0)
    {
        printf("java_jni.c:processStatus - no constructor?!\n");
        return 0;
    }

    obj = (*env)->NewObject(env, exClass, exConstructor, status);
    if (obj == 0)
    {
        printf("java_jni.c:processStatus - no object?!\n");
        return 0;
    }

    if ((*env)->Throw(env, obj) < 0)
    {
        printf("java_jni.c:processStatus - failed to throw?!\n");
        return 0;
    }
    return 0;
}

jobject processStatusReturnCryptQueryInfo(JNIEnv *env, int status, CRYPT_QUERY_INFO returnValue)
{
    jclass exClass;
    jmethodID exConstructor;
    jstring algoName;
    jobject obj;

    if (status < cryptlib_crypt_OK)
        return NULL;

    exClass = (*env)->FindClass(env, "cryptlib/CRYPT_QUERY_INFO");
    if (exClass == 0)
    {
        printf("java_jni.c:processStatusReturnCryptQueryInfo - no class?!\n");
        return NULL;
    }

    exConstructor = (*env)->GetMethodID(env, exClass, "<init>", "(Ljava/lang/String;IIII)V");
    if (exConstructor == 0)
    {
        printf("java_jni.c:processStatusReturnCryptQueryInfo - no constructor?!\n");
        return NULL;
    }

    algoName = (*env)->NewStringUTF(env, returnValue.algoName);

    obj = (*env)->NewObject(env, exClass, exConstructor, algoName, returnValue.blockSize, returnValue.minKeySize, returnValue.keySize, returnValue.maxKeySize);
    if (obj == 0)
    {
        printf("java_jni.c:processStatusReturnCryptQueryInfo - no object?!\n");
        return NULL;
    }

    return obj;
}

jobject processStatusReturnCryptObjectInfo(JNIEnv *env, int status, CRYPT_OBJECT_INFO returnValue)
{
    jclass exClass;
    jmethodID exConstructor;
    jbyteArray salt;
    jobject obj;

    if (status < cryptlib_crypt_OK)
        return NULL;

    exClass = (*env)->FindClass(env, "cryptlib/CRYPT_OBJECT_INFO");
    if (exClass == 0)
    {
        printf("java_jni.c:processStatusReturnCryptObjectInfo - no class?!\n");
        return NULL;
    }

    exConstructor = (*env)->GetMethodID(env, exClass, "<init>", "(IIII[B)V");
    if (exConstructor == 0)
    {
        printf("java_jni.c:processStatusReturnCryptObjectInfo - no constructor?!\n");
        return NULL;
    }

    salt = (*env)->NewByteArray(env, returnValue.saltSize);
    (*env)->SetByteArrayRegion(env, salt, 0, returnValue.saltSize, returnValue.salt);

    obj = (*env)->NewObject(env, exClass, exConstructor, returnValue.objectType, returnValue.cryptAlgo, returnValue.cryptMode, returnValue.hashAlgo, salt);
    if (obj == 0)
    {
        printf("java_jni.c:processStatusReturnCryptObjectInfo - no object?!\n");
        return NULL;
    }

    return obj;
}

int checkIndicesArray(JNIEnv *env, jbyteArray array, int sequenceOffset, int sequenceLength)
{
    jsize arrayLength;
    jclass exClass;

    if (array == NULL)
    {
        if (sequenceOffset == 0)
            return 1;
        else
        {
            exClass = (*env)->FindClass(env, "java/lang/ArrayIndexOutOfBoundsException");
            if (exClass == 0)
                printf("java_jni.c:checkIndicesArray - no class?!\n");
            else
            if ((*env)->ThrowNew(env, exClass, "") < 0)
                printf("java_jni.c:checkIndicesArray - failed to throw?!\n");
            return 0;
        }
    }

    arrayLength = (*env)->GetArrayLength(env, array);

    if (sequenceOffset < 0 ||
        sequenceOffset >= arrayLength ||
        sequenceOffset + sequenceLength > arrayLength)
    {
        exClass = (*env)->FindClass(env, "java/lang/ArrayIndexOutOfBoundsException");
        if (exClass == 0)
            printf("java_jni.c:checkIndicesArray - no class?!\n");
        else
        if ((*env)->ThrowNew(env, exClass, "") < 0)
            printf("java_jni.c:checkIndicesArray - failed to throw?!\n");
        return 0;
    }
    return 1;
}

int getPointerArray(JNIEnv* env, jbyteArray array, jbyte** bytesPtrPtr)
{
    jboolean isCopy;

    if (array == NULL)
    {
        (*bytesPtrPtr) = NULL;
        return 1;
    }

    (*bytesPtrPtr) = (*env)->GetByteArrayElements(env, array, &isCopy);

    if (*bytesPtrPtr == NULL)
    {
        printf("java_jni.c:getPointer - failed to get elements of array?!\n");
        return 0;
    }
    return 1;
}

void releasePointerArray(JNIEnv* env,jbyteArray array, jbyte* bytesPtr)
{
    if (bytesPtr == NULL)
        return;
    (*env)->ReleaseByteArrayElements(env, array, bytesPtr, 0);
}

int checkIndicesNIO(JNIEnv *env, jobject byteBuffer, int sequenceOffset, int sequenceLength)
{
    jlong byteBufferLength;
    jclass exClass;

    if (byteBuffer == NULL)
    {
        if (sequenceOffset == 0)
            return 1;
        else
        {
            exClass = (*env)->FindClass(env, "java/lang/ArrayIndexOutOfBoundsException");
            if (exClass == 0)
                printf("java_jni.c:checkIndicesNIO - no class?!\n");
            else
            if ((*env)->ThrowNew(env, exClass, "") < 0)
                printf("java_jni.c:checkIndicesNIO - failed to throw?!\n");
            return 0;
        }
    }

    byteBufferLength = (*env)->GetDirectBufferCapacity(env, byteBuffer);
    if (byteBufferLength == -1)
    {
        exClass = (*env)->FindClass(env, "java/lang/UnsupportedOperationException");
        if (exClass == 0)
            printf("java_jni.c:checkIndicesNIO - no class?!\n");
        else
        if ((*env)->ThrowNew(env, exClass,
"Either a non-direct ByteBuffer was passed or your JVM doesn't support JNI access to direct ByteBuffers") < 0)
            printf("java_jni.c:checkIndicesNIO - failed to throw?!\n");
        return 0;
    }

    if (sequenceOffset < 0 ||
        sequenceOffset >= byteBufferLength ||
        sequenceOffset + sequenceLength > byteBufferLength)
    {
        exClass = (*env)->FindClass(env, "java/lang/ArrayIndexOutOfBoundsException");
        if (exClass == 0)
            printf("java_jni.c:checkIndicesNIO - no class?!\n");
        else
        if ((*env)->ThrowNew(env, exClass, "") < 0)
            printf("java_jni.c:checkIndicesNIO - failed to throw?!\n");
        return 0;
    }
    return 1;
}

int getPointerNIO(JNIEnv* env, jobject byteBuffer, jbyte** bytesPtrPtr)
{
    jclass exClass;

    if (byteBuffer == NULL)
    {
        (*bytesPtrPtr) = NULL;
        return 1;
    }

    (*bytesPtrPtr) = (*env)->GetDirectBufferAddress(env, byteBuffer);

    if (*bytesPtrPtr == NULL)
    {
        exClass = (*env)->FindClass(env, "java/lang/UnsupportedOperationException");
        if (exClass == 0)
            printf("java_jni.c:getPointerNIO - no class?!\n");
        else
        if ((*env)->ThrowNew(env, exClass, "Your JVM doesn't support JNI access to direct ByteBuffers") < 0)
            printf("java_jni.c:getPointerNIO - failed to throw?!\n");
        return 0;
    }
    return 1;
}

void releasePointerNIO(JNIEnv* env,jbyteArray array, jbyte* bytesPtr)
{
}

int getPointerString(JNIEnv* env, jstring str, jbyte** bytesPtrPtr)
{
   jboolean isCopy;
   jsize strLength;
   const jbyte* rawBytesPtr;
   jclass exClass;
#ifdef __WINCE__
   int status;
#endif // __WINCE__


   if (str == NULL)
   {
       (*bytesPtrPtr) = NULL;
       return 1;
   }

   rawBytesPtr = (*env)->GetStringUTFChars(env, str, &isCopy);

   if (rawBytesPtr == NULL)
   {
       printf("java_jni.c:getPointerString - failed to get elements of String?!\n");
       return 0;
   }

   strLength = (*env)->GetStringUTFLength(env, str);

#ifdef __WINCE__
   (*bytesPtrPtr) = (jbyte*)malloc(strLength*2+2); // this is unicode, therefore \0 is two bytes long
#else
   (*bytesPtrPtr) = (jbyte*)malloc(strLength+1);
#endif // __WINCE__

   if (*bytesPtrPtr == NULL)
   {
       exClass = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
       if (exClass == 0)
           printf("java_jni.c:getPointerString - no class?!\n");
       else
       if ((*env)->ThrowNew(env, exClass, "") < 0)
           printf("java_jni.c:getPointerString - failed to throw?!\n");
       (*env)->ReleaseStringUTFChars(env, str, rawBytesPtr);
       return 0;
   }

#ifdef __WINCE__
   status = asciiToUnicode (*bytesPtrPtr, strLength*2+2, rawBytesPtr, strLength+1);
   if (status == CRYPT_ERROR_BADDATA) {
       (*env)->ReleaseStringUTFChars(env, str, rawBytesPtr);
       return 0;
   }
#else
   memcpy(*bytesPtrPtr, rawBytesPtr, strLength);
   (*bytesPtrPtr)[strLength] = 0;
#endif // __WINCE__

   (*env)->ReleaseStringUTFChars(env, str, rawBytesPtr);

   return 1;
}

void releasePointerString(JNIEnv* env, jstring str, jbyte* bytesPtr)
{
    if (bytesPtr == NULL)
        return;
    free(bytesPtr);
}



#endif /* USE_JAVA */
