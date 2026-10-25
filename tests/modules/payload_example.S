/*
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

.global _start
.section .text

_start:
    /*
     * Deterministic test payload:
     * Write 64-bit magic signature 0x474E554D41434821 ("GNUMACH!")
     * to physical RAM address 0x40001000.
     */
    ldr x0, =0x474E554D41434821
    ldr x1, =0x40001000
    str x0, [x1]

    /*
     * Write 32-bit status code 0xCAFEBABE to 0x40001008.
     */
    ldr w2, =0xCAFEBABE
    str w2, [x1, #8]

halt_loop:
    wfe
    b halt_loop
