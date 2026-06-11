# RISC-V machine-mode (PMP) profile

This architecture profile (`ARCH_PROFILE=riscv-m`) lets Bao act as a
static-partitioning monitor on RISC-V cores that **lack the hypervisor (H)
extension**. Instead of 2-stage MMU translation, partitions are isolated with
**Physical Memory Protection (PMP)**.

It is the RISC-V analogue of the `armv8-r` (MPU) profile: it reuses the core
memory-protection backend in `src/core/mpu/` and provides a PMP driver as the
arch-side implementation of the `mpu_init`/`mpu_map`/`mpu_unmap` hooks.

## Privilege model

| Privilege | Software                                                |
| --------- | ------------------------------------------------------- |
| M-mode    | Bao monitor (owns `mtvec`) + resident OpenSBI service   |
| S-mode    | Guest partition OS                                      |
| U-mode    | Guest partition userspace                               |

- Bao runs in **M-mode** and owns the machine trap vector (`mtvec`).
- Each guest runs natively in **S/U-mode**; static partitioning assigns real
  hardware to a partition and PMP enforces its physical boundaries. There is no
  per-instruction trap-and-emulate.
- `medeleg`/`mideleg` are programmed so that traps and interrupts a partition
  can legitimately handle on its own are delegated straight to its S-mode,
  minimizing M-mode entries.
- A guest's `ecall` from S-mode is **not** delegated and traps to Bao's M-mode
  SBI server.

## SBI handling

Bao splits SBI into two halves:

- **Guest SBI server** (`sbi_guest`): handles partition-stateful extensions
  itself — `TIME` (per-partition timer via `mtimecmp` + `mip.STIP`
  injection), `IPI` (inter-hart `msip`), `HSM` (PMP-aware hart bring-up),
  plus `BASE`, `RFENCE` and `DBCN`.
- **OpenSBI backend client** (`sbi_backend`): relays the *generic*,
  partition-agnostic services (console, base/probe, system reset, and
  optionally remote fences) to a separately-built **resident OpenSBI**.

## OpenSBI co-residency ABI (handoff + relay)

Because Bao is the M-mode owner, OpenSBI is used as a **resident service**, not
as the boot firmware that drops to S-mode. The contract:

1. **Boot handoff.** OpenSBI cold-boots in M-mode and performs platform init,
   errata and PMP reset as usual. Its boot tail is patched so that, instead of
   switching to S-mode and entering the payload, it **stays in M-mode and jumps
   to Bao**, publishing its service entry point and per-hart scratch pointer to
   Bao via a fixed register ABI:

   | Reg  | Meaning                                                      |
   | ---- | ----------------------------------------------------------- |
   | `a0` | hart id                                                      |
   | `a1` | Bao config blob address (unchanged from the normal payload) |
   | `a2` | resident OpenSBI service entry point                        |
   | `a3` | OpenSBI per-hart scratch base                               |

2. **Bao takes over.** Bao installs its own `mtvec`, configures PMP, and runs
   the partitions.

3. **Service relay.** When Bao needs a generic SBI service, it performs a
   synchronous **function call** into the published OpenSBI entry with the
   standard SBI argument convention (`a7`=extid, `a6`=fid, `a0..a5`=args),
   using the saved scratch pointer. Only stateless/platform services are
   relayed this way; nothing that touches partition scheduling state is.

The OpenSBI-side patch implementing the boot handoff lives in a separate
repository and is out of scope for this tree; this profile only needs the
fixed entry/ABI above to be honored.

> Status: **work in progress.** Sources are landing incrementally; see
> `objects.mk` for the planned compilation units.
