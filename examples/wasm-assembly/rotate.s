	.text
	.functype	wasm_rotl32 (i32, i32) -> (i32)
	.globl	wasm_rotl32
	.type	wasm_rotl32,@function
wasm_rotl32:
	.functype	wasm_rotl32 (i32, i32) -> (i32)
	local.get	0
	local.get	1
	i32.rotl
	end_function
