;; extern void encode_hex256(char* in, char* out);

;; RDI is address of buf to encode (binary). Must be 32 bytes.
;; RSI is the address of the output (must be 64 bytes)
;; If this is ported to windows, the calling convention is RCX, RDX for the first two params

section   .text

global encode_hex256

encode_hex256:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;; ymm{0,1}: values to encode; 4 bits per byte
;;; ymm2: mask for low bytes
;;; ymm3: mask for high bytes
;;; ymm{4,5}: low nibble of bytes to encode
;;; ymm{6,7}: high nibble of bytes to encode

  vmovdqu xmm4, [rdi]
  vmovdqu xmm5, [rdi+16]
  vpmovzxbw ymm4, xmm4
  vpmovzxbw ymm5, xmm5
  vpbroadcastw ymm2, [lownibble]
  vpsllw ymm3, ymm2, 4          ; highnibble
  vpand ymm6, ymm4, ymm3
  vpand ymm7, ymm5, ymm3
  vpand ymm4, ymm4, ymm2
  vpand ymm5, ymm5, ymm2
  vpsllw ymm4, ymm4, 8
  vpsllw ymm5, ymm5, 8
  vpsrlw ymm6, ymm6, 4
  vpsrlw ymm7, ymm7, 4
  vpaddb ymm0, ymm4, ymm6
  vpaddb ymm1, ymm5, ymm7
;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Replace values with hex chars
;;; ymm{0,1} starts as from above. Transformed to hex chars
;;; ymm2 all bytes 10
;;; ymm{3,4} mask of all values less than 10
;;; ymm5 all bytes 48
;;; ymm6 all bytes 55
;;; ymm{7,8} values to add to the input to get hex chars (either 48 or 55 depending if the value < 10)

  vpbroadcastb ymm2, [ten]
  vpcmpgtb ymm3, ymm2, ymm0
  vpcmpgtb ymm4, ymm2, ymm1
  vpbroadcastb ymm5, [fourtyeight]
  vpbroadcastb ymm6, [fiftyfive]
  vpblendvb ymm7, ymm6, ymm5, ymm3
  vpblendvb ymm8, ymm6, ymm5, ymm4
  vpaddb ymm0, ymm0, ymm7
  vpaddb ymm1, ymm1, ymm8

;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  ; save the result
  vmovdqu [rsi], ymm0
  vmovdqu [rsi+32], ymm1

  vzeroupper
  ret

section   .data align=32               ; align on 256 bit boundary for avx2 instructions
ten: db 10
fourtyeight: db 48
fiftyfive: db 55
lownibble: db 0x0f, 0x00
