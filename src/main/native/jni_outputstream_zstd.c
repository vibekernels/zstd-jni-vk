#ifndef ZSTD_STATIC_LINKING_ONLY
#define ZSTD_STATIC_LINKING_ONLY
#endif
#include <jni.h>
#include <zstd.h>
#include <zstd_errors.h>
#include <stdlib.h>
#include <stdint.h>

/* field IDs can't change in the same VM */
static jfieldID src_pos_id;
static jfieldID dst_pos_id;

/*
 * Class:     com_github_luben_zstd_ZstdOutputStreamNoFinalizer
 * Method:    recommendedCOutSize
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_com_github_luben_zstd_ZstdOutputStreamNoFinalizer_recommendedCOutSize
  (JNIEnv *env, jclass obj) {
    return (jlong) ZSTD_CStreamOutSize();
}

/*
 * Class:     com_github_luben_zstd_ZstdOutputStreamNoFinalizer
 * Method:    createCStream
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_com_github_luben_zstd_ZstdOutputStreamNoFinalizer_createCStream
  (JNIEnv *env, jclass obj) {
    return (jlong)(intptr_t) ZSTD_createCStream();
}

/*
 * Class:     com_github_luben_zstd_ZstdOutputStreamNoFinalizer
 * Method:    freeCStream
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_com_github_luben_zstd_ZstdOutputStreamNoFinalizer_freeCStream
  (JNIEnv *env, jclass obj, jlong stream) {
    return ZSTD_freeCStream((ZSTD_CStream *)(intptr_t) stream);
}

/*
 * Class:     com_github_luben_zstd_ZstdOutputStreamNoFinalizer
 * Method:    resetCStream
 * Signature: (JII)I
 */
JNIEXPORT jint JNICALL Java_com_github_luben_zstd_ZstdOutputStreamNoFinalizer_resetCStream
  (JNIEnv *env, jclass obj, jlong stream) {
    jclass clazz = (*env)->GetObjectClass(env, obj);
    src_pos_id = (*env)->GetFieldID(env, clazz, "srcPos", "J");
    dst_pos_id = (*env)->GetFieldID(env, clazz, "dstPos", "J");
    return ZSTD_CCtx_reset((ZSTD_CStream *)(intptr_t) stream, ZSTD_reset_session_only);
}

/*
 * Class:     com_github_luben_zstd_ZstdOutputStreamNoFinalizer
 * Method:    compressStream
 * Signature: (J[BI[BI)I
 */
JNIEXPORT jint JNICALL Java_com_github_luben_zstd_ZstdOutputStreamNoFinalizer_compressStream
  (JNIEnv *env, jclass obj, jlong stream, jbyteArray dst, jint dst_size, jbyteArray src, jint src_size) {

    size_t size = -ZSTD_error_memory_allocation;

    size_t src_pos = (size_t) (*env)->GetLongField(env, obj, src_pos_id);
    void *dst_buff = (*env)->GetPrimitiveArrayCritical(env, dst, NULL);
    if (dst_buff == NULL) goto E1;
    void *src_buff = (*env)->GetPrimitiveArrayCritical(env, src, NULL);
    if (src_buff == NULL) goto E2;

    ZSTD_outBuffer output = { dst_buff, dst_size, 0 };
    ZSTD_inBuffer input = { src_buff, src_size, src_pos };

    size = ZSTD_compressStream2((ZSTD_CStream *)(intptr_t) stream, &output, &input, ZSTD_e_continue);

    size_t final_src_pos = input.pos;
    size_t final_dst_pos = output.pos;
    (*env)->ReleasePrimitiveArrayCritical(env, src, src_buff, JNI_ABORT);
E2: (*env)->ReleasePrimitiveArrayCritical(env, dst, dst_buff, 0);
    (*env)->SetLongField(env, obj, src_pos_id, final_src_pos);
    (*env)->SetLongField(env, obj, dst_pos_id, final_dst_pos);

E1: return (jint) size;
}

/*
 * Class:     com_github_luben_zstd_ZstdOutputStreamNoFinalizer
 * Method:    endStream
 * Signature: (J[BI)I
 */
JNIEXPORT jint JNICALL Java_com_github_luben_zstd_ZstdOutputStreamNoFinalizer_endStream
  (JNIEnv *env, jclass obj, jlong stream, jbyteArray dst, jint dst_size) {

    size_t size = -ZSTD_error_memory_allocation;
    void *dst_buff = (*env)->GetPrimitiveArrayCritical(env, dst, NULL);
    if (dst_buff != NULL) {
        ZSTD_outBuffer output = { dst_buff, dst_size, 0 };
        ZSTD_inBuffer input = {NULL, 0, 0};
        size = ZSTD_compressStream2((ZSTD_CStream *)(intptr_t) stream, &output, &input, ZSTD_e_end);
        (*env)->ReleasePrimitiveArrayCritical(env, dst, dst_buff, 0);
        (*env)->SetLongField(env, obj, dst_pos_id, output.pos);
    }
    return (jint) size;
}

/*
 * Class:     com_github_luben_zstd_ZstdOutputStreamNoFinalizer
 * Method:    compressFrameEnd
 * Signature: (J[BI[BII)I
 *
 * Single-shot compression: reset context, enable stableInBuffer (zero-copy
 * from Java array), compress all input with ZSTD_e_end, and close the frame
 * in a single JNI call. The output buffer must be >= ZSTD_compressBound(len)
 * to guarantee completion in one call.
 */
JNIEXPORT jint JNICALL Java_com_github_luben_zstd_ZstdOutputStreamNoFinalizer_compressFrameEnd
  (JNIEnv *env, jclass obj, jlong stream, jbyteArray dst, jint dst_size, jbyteArray src, jint src_offset, jint src_size) {

    ZSTD_CCtx *cctx = (ZSTD_CCtx *)(intptr_t) stream;
    size_t size = -ZSTD_error_memory_allocation;

    /* Ensure field IDs are initialized (may be first call on this context) */
    if (dst_pos_id == NULL) {
        jclass clazz = (*env)->GetObjectClass(env, obj);
        src_pos_id = (*env)->GetFieldID(env, clazz, "srcPos", "J");
        dst_pos_id = (*env)->GetFieldID(env, clazz, "dstPos", "J");
    }

    /* Reset session for the new frame */
    size_t rc = ZSTD_CCtx_reset(cctx, ZSTD_reset_session_only);
    if (ZSTD_isError(rc)) return (jint) rc;

    /* Enable stableInBuffer: ZSTD reads directly from the pinned Java array
     * instead of copying input to an internal buffer. Safe because the entire
     * frame is compressed within this single critical section. */
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_stableInBuffer, 1);

    void *dst_buff = (*env)->GetPrimitiveArrayCritical(env, dst, NULL);
    if (dst_buff == NULL) goto E1;
    void *src_buff = (*env)->GetPrimitiveArrayCritical(env, src, NULL);
    if (src_buff == NULL) goto E2;

    ZSTD_outBuffer output = { dst_buff, dst_size, 0 };
    ZSTD_inBuffer input = { src_buff, src_size, src_offset };

    size = ZSTD_compressStream2(cctx, &output, &input, ZSTD_e_end);

    size_t final_dst_pos = output.pos;
    (*env)->ReleasePrimitiveArrayCritical(env, src, src_buff, JNI_ABORT);
E2: (*env)->ReleasePrimitiveArrayCritical(env, dst, dst_buff, 0);
    (*env)->SetLongField(env, obj, dst_pos_id, final_dst_pos);

    /* Disable stableInBuffer so the context can be reused normally */
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_stableInBuffer, 0);

E1: return (jint) size;
}

/*
 * Class:     com_github_luben_zstd_ZstdOutputStreamNoFinalizer
 * Method:    flushStream
 * Signature: (J[BI)I
 */
JNIEXPORT jint JNICALL Java_com_github_luben_zstd_ZstdOutputStreamNoFinalizer_flushStream
  (JNIEnv *env, jclass obj, jlong stream, jbyteArray dst, jint dst_size) {

    size_t size = -ZSTD_error_memory_allocation;
    void *dst_buff = (*env)->GetPrimitiveArrayCritical(env, dst, NULL);
    if (dst_buff != NULL) {
        ZSTD_outBuffer output = { dst_buff, dst_size, 0 };
        ZSTD_inBuffer input = {NULL, 0, 0};
        size = ZSTD_compressStream2((ZSTD_CStream *)(intptr_t) stream, &output, &input, ZSTD_e_flush);
        (*env)->ReleasePrimitiveArrayCritical(env, dst, dst_buff, 0);
        (*env)->SetLongField(env, obj, dst_pos_id, output.pos);
    }
    return (jint) size;
}
