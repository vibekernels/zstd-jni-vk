# CLAUDE.md

## Project

zstd-jni: JNI bindings for Zstd compression library. Built with SBT + Scala tests, native code compiled via gcc.

## Build & Test

```bash
# Full build and test
./sbt compile test package

# Run all performance benchmarks
./sbt "testOnly com.github.luben.zstd.ZstdPerfSpec"
```

## Benchmark: Streaming with BufferPool (level 3)

This is the HTTP-relevant benchmark (streaming compression/decompression with buffer reuse).

```bash
./bench.sh
```

Runs `ZstdPerfStreamingBufferPoolSpec` — 200 cycles over ~5MB XML data at compression level 3, with A/B comparison (BASELINE vs OPTIMIZED). The script patches build files to work around a missing `sbt-java-module-info` plugin and restores them on exit.

- Test file: `src/test/scala/PerfStreamingBufferPool.scala`
- Runner script: `bench.sh`
- Requires: Java 21 (Corretto) at `/usr/lib/jvm/java-21-amazon-corretto`, gcc
- CPU: Intel Xeon Platinum 8259CL (Cascade Lake), GCC 11.5.0

## Optimization Dead Ends (don't retry)

- **`-march=x86-64-v3`**: AVX2 frequency throttling on Cascade Lake causes 35% regression
- **`-funroll-loops`**: Bloats code size, hurts icache for ZSTD's tight loops (~5% regression)
- **PGO (`-fprofile-generate`/`-fprofile-use`)**: Even with correct gcda paths, degraded performance vs baseline
- **ZSTD compile-time specialization** (`HUF_FORCE_DECOMPRESS_X2`, `ZSTD_FORCE_DECOMPRESS_SEQUENCES_SHORT`): Locks in one algorithm, not suitable for a library
- **JNI field access optimization**: Already cached in static variables, no gain possible
- **Context reuse alone (without API changes)**: Eliminating ZSTD_createDCtx/freeDCtx per cycle saves only ~0.1% — malloc/free overhead is negligible vs actual compression/decompression time
- **`ZSTD_decompressStream` for single-shot decompression**: Streaming API has per-block state machine overhead; `ZSTD_decompressDCtx` (simple API) is measurably faster for single-frame decompression
- **Standalone benchmark without JIT warmup**: Run-to-run variance exceeds 5%; always use A/B comparison within the same JVM run for reliable measurement
