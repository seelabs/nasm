;; extern int base58_8_coeff(unsigned char const* in, std::uint64_t* out, unsigned int const* alphabet);

;; RDI is address of buf to decode (base58 rippled string). Must be 44 bytes.
;; RSI is the address of the output (must be 6 qwords)
;; RDX is the address of the alphabet (must be 256 dwords, not bytes since vpgather can't handle bytes)

section   .text

global base58_8_coeff

base58_8_coeff:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Initialize constants
;;;
;;; xmm12: all lanes 64-bit 58^4
;;; ymm13: all bits cleared
;;; ymm14: base58 powers from 0 to three in both lanes
;;; ymm15: all bits set

  vpcmpeqd ymm15, ymm15
  vmovdqa ymm14, [base58_powers_0_to_3]
  vpxor ymm13, ymm13, ymm13
  vpbroadcastq xmm12, [base58_powers_4]

;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Convert input to values through the LUT
;;; ymm{0,1}: values for first 16 input digits

  ; input is big endian
  vmovdqu xmm6, [rdi+36]
  vmovdqu xmm7, [rdi+28]
  vmovdqu xmm8, [rdi+20]
  vmovdqu xmm9, [rdi+12]
  vmovdqu xmm10, [rdi+4]
  vpmovzxbd ymm6, xmm6           ; zero extend 8 bytes in xmm0 to 8 double words in ymm0
  vpmovzxbd ymm7, xmm7
  vpmovzxbd ymm8, xmm8
  vpmovzxbd ymm9, xmm9
  vpmovzxbd ymm10, xmm10
  ;; use a lut to convert the alpha-numeric chars to values
  vpgatherdd ymm0, [rdx + ymm6 * 4], ymm15
  vpcmpeqd ymm15, ymm15         ; the previous instruction clears ymm15
  vpgatherdd ymm1, [rdx + ymm7 * 4], ymm15
  vpcmpeqd ymm15, ymm15
  vpgatherdd ymm2, [rdx + ymm8 * 4], ymm15
  vpcmpeqd ymm15, ymm15
  vpgatherdd ymm3, [rdx + ymm9 * 4], ymm15
  vpcmpeqd ymm15, ymm15
  vpgatherdd ymm4, [rdx + ymm10 * 4], ymm15
  vpcmpeqd ymm15, ymm15

  ;; Put the first four characters in the high lane of ymm5, low lane zero (to make computations regular)
  vpxor xmm6, xmm6
  movd xmm6, [rdi]
  vpmovzxbd ymm6, xmm6           ; zero extend 8 bytes in xmm0 to 8 double words in ymm0 (high lane will be all zero)
  vpgatherdd xmm5, [rdx + xmm6 * 4], xmm15
  ; put low lane in high lane, clear low lane
  vpxor ymm6, ymm6
  vinserti128 ymm5, ymm6, xmm5, 1 ; Insert from the low lane of xmm5 into the high lane of ymm6 and store in ymm5
  vpcmpeqd ymm15, ymm15
;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;; TBD: Error check that all the values are in range

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Multiply each set of 4 values by 58^0, 58^1, 58^2, 58^3

  vpmulld ymm0, ymm0, ymm14      ; This is signed multiplication, but it won't overflow
  vpmulld ymm1, ymm1, ymm14
  vpmulld ymm2, ymm2, ymm14
  vpmulld ymm3, ymm3, ymm14
  vpmulld ymm4, ymm4, ymm14
  vpmulld ymm5, ymm5, ymm14
;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Horizonal add
;;; ymm0: 4 64-bit values.
;;;       The high lane has the sum of the high four values from ymm1 followed by the sum of the high four values from ymm0
;;;       The low lane has the sum of the low four values from ymm1 followed by the sum of the low four values from ymm0

  vphaddd ymm0, ymm0, ymm13
  vphaddd ymm1, ymm1, ymm13
  vphaddd ymm0, ymm0, ymm1

  vphaddd ymm2, ymm2, ymm13
  vphaddd ymm3, ymm3, ymm13
  vphaddd ymm2, ymm2, ymm3

  vphaddd ymm4, ymm4, ymm13
  vphaddd ymm5, ymm5, ymm13
  vphaddd ymm4, ymm4, ymm5

;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Multiply the high words by 58^4 and add to the low words

  vextracti128 xmm1, ymm0, 1      ; Extract the high lane of ymm0 and put it into xmm1
  vpmuludq xmm0, xmm0, xmm12
  vpaddq xmm0, xmm0, xmm1       ; xmm0 contains the coefficients 0 and 1

  vextracti128 xmm3, ymm2, 1
  vpmuludq xmm2, xmm2, xmm12
  vpaddq xmm2, xmm2, xmm3       ; xmm2 contains coefficients 2 and 3

  vextracti128 xmm5, ymm4, 1
  vpmuludq xmm4, xmm4, xmm12
  vpaddq xmm4, xmm4, xmm5       ; xmm4 contains coefficients 4 and 5

;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  vmovdqu [rsi], xmm0
  vmovdqu [rsi+16], xmm2
  vmovdqu [rsi+32], xmm4
  mov eax, 1
  vzeroupper
  ret

section   .data align=32               ; align on 256 bit boundary for avx2 instructions
base58_powers_0_to_3: dd 0x2FA28, 0xD24, 0x3A, 0x1, 0x2FA28, 0xD24, 0x3A, 0x1 
base58_powers_4: dq 0xACAD10    ; 58^4
