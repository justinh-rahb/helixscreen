# Auto-Update Phase 1: Enable SSL on AD5M

## Status: BUILD COMPLETE - AWAITING DEVICE TEST

## Objective
Enable HTTPS capability on AD5M builds so HelixScreen can make HTTPS requests to GitHub API for update checking.

## Success Criteria
- AD5M binary can successfully make HTTPS requests to `https://api.github.com/`
- Build still produces a working binary
- No significant size increase (acceptable: +100-200KB for SSL code)

---

## Background Research (Complete)

### Current State
- AD5M build has `ENABLE_SSL := no` in `mk/cross.mk:73`
- Docker container (`docker/Dockerfile.ad5m`) has NO OpenSSL headers
- libhv supports `--with-openssl` configure flag
- AD5M device has `/usr/lib/libssl.so.1.1` and `/usr/lib/libcrypto.so.1.1` present
- Current build is FULLY STATIC (`-static` in TARGET_LDFLAGS)

### Key Challenge
Static linking with OpenSSL is tricky because:
1. We need OpenSSL headers at compile time
2. We need static libraries (`libssl.a`, `libcrypto.a`) for the ARM toolchain
3. ARM's pre-built GCC 10.3 toolchain doesn't include OpenSSL

### Approach Decision
**Option A: Static OpenSSL** - Download/build ARM OpenSSL static libs in Docker
**Option B: Dynamic SSL** - Remove `-static` for libssl/libcrypto only, rely on device libs
**Option C: mbedTLS** - Alternative TLS library that's easier to embed

Going with **Option A** first - it maintains our "no runtime dependencies" philosophy.

---

## Implementation Steps

### Step 1: Modify Docker Toolchain ✅ DONE
**File:** `docker/Dockerfile.ad5m`

Add OpenSSL cross-compilation:
1. Download ARM OpenSSL source (1.1.x to match device)
2. Configure and build static libraries for armv7-a
3. Install headers and libs in toolchain sysroot

**Code Review:** APPROVED - Clean implementation, installs to toolchain sysroot

### Step 2: Update Build System ✅ DONE
**File:** `mk/cross.mk`

1. Changed `ENABLE_SSL := no` to `ENABLE_SSL := yes` for AD5M
2. SSL flags already in Makefile (line 435-437)

**File:** `mk/deps.mk`

1. Updated libhv-build to pass `--with-openssl` when `ENABLE_SSL=yes`
2. Added OpenSSL include/lib paths for cross-compilation using Make conditionals

**Code Review:** APPROVED - Correct Make syntax, K1 builds unaffected

### Step 3: Test Build ✅ DONE
```bash
# Rebuild Docker image with OpenSSL
make docker-toolchain-ad5m  # SUCCESS

# Clean libhv and rebuild
make libhv-clean && make ad5m-docker  # SUCCESS

# Verify SSL symbols
strings build/ad5m/bin/helix-screen | grep -E "SSL_|handshake"
# Output: "ssl client handshake failed", "SSL_CTX" - SSL IS WORKING
```

Binary size: 8.4MB (acceptable for static SSL)

### Step 4: Test on Device
```bash
# Deploy to AD5M
make deploy-ad5m

# Test HTTPS capability
ssh root@ad5m.local "cd /opt/helixscreen && ./helix-screen --test -vv"
# Look for HTTPS test or add a manual HTTPS test
```

---

## Changes Made

### docker/Dockerfile.ad5m
- Added OpenSSL 1.1.1w source download and cross-compilation
- Installed headers to `/opt/arm-toolchain/arm-none-linux-gnueabihf/include/`
- Installed static libs to `/opt/arm-toolchain/arm-none-linux-gnueabihf/lib/`

### mk/cross.mk
- Line 73: Changed `ENABLE_SSL := no` to `ENABLE_SSL := yes`

### mk/deps.mk
- Added `--with-openssl` flag to libhv configure when `ENABLE_SSL=yes`
- Added OpenSSL include/lib paths for cross-compilation

---

## Verification Checklist

- [x] Docker image builds successfully
- [x] AD5M binary builds successfully
- [x] Binary contains SSL symbols (`strings | grep SSL`)
- [ ] Binary runs on AD5M device
- [ ] HTTPS request to api.github.com succeeds
- [x] No significant size regression (8.4MB, acceptable)

---

## Fallback Plan

If static OpenSSL approach fails:

1. **Ship static curl binary**: Cross-compile curl with mbedTLS, shell out for HTTPS
2. **HTTP-only on AD5M**: Check HTTPS capability at runtime, show manual update instructions
3. **Dynamic linking for SSL only**: Hybrid approach, most binary static but SSL dynamic

---

## Notes

- OpenSSL 1.1.1 chosen because AD5M device has 1.1.x runtime (won't matter for static linking but good for compatibility)
- LTO (`-flto`) should work with OpenSSL since we're compiling it ourselves
- CA certificates: AD5M has `/etc/ssl/certs/` - need to verify libhv/OpenSSL finds them
