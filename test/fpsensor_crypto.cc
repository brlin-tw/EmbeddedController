/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fpsensor/fpsensor_crypto.h"
#include "fpsensor/fpsensor_state_without_driver_info.h"

#include <array>
#include <vector>

extern "C" {
#include "sha256.h"
#include "test_util.h"
}

struct HkdfTestVector {
	std::vector<uint8_t> ikm;
	std::vector<uint8_t> salt;
	std::vector<uint8_t> info;
	std::vector<uint8_t> prk;
	std::vector<uint8_t> okm;
};

extern "C" enum ec_error_list rollback_get_secret(uint8_t *secret)
{
	// We should not call this function in the test.
	TEST_ASSERT(false);
}

test_static int test_hkdf_expand(void)
{
	/* Test vectors from
	 * https://datatracker.ietf.org/doc/html/rfc5869#appendix-A */

	/* https://datatracker.ietf.org/doc/html/rfc5869#appendix-A.1 */
	const HkdfTestVector test_vector1 = {
		.info = {
			0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
			0xf9,
		},
		.prk = {
			0x07, 0x77, 0x09, 0x36, 0x2c, 0x2e, 0x32, 0xdf, 0x0d,
			0xdc, 0x3f, 0x0d, 0xc4, 0x7b, 0xba, 0x63, 0x90, 0xb6,
			0xc7, 0x3b, 0xb5, 0x0f, 0x9c, 0x31, 0x22, 0xec, 0x84,
			0x4a, 0xd7, 0xc2, 0xb3, 0xe5,
		},
		.okm = {
			0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a, 0x90,
			0x43, 0x4f, 0x64, 0xd0, 0x36, 0x2f, 0x2a, 0x2d, 0x2d,
			0x0a, 0x90, 0xcf, 0x1a, 0x5a, 0x4c, 0x5d, 0xb0, 0x2d,
			0x56, 0xec, 0xc4, 0xc5, 0xbf, 0x34, 0x00, 0x72, 0x08,
			0xd5, 0xb8, 0x87, 0x18, 0x58, 0x65,
		},
	};

	/* https://datatracker.ietf.org/doc/html/rfc5869#appendix-A.2 */
	const HkdfTestVector test_vector2 = {
		.info = {
			0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
			0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1,
			0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
			0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3,
			0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc,
			0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5,
			0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee,
			0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
			0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
		},
		.prk = {
			0x06, 0xa6, 0xb8, 0x8c, 0x58, 0x53, 0x36, 0x1a, 0x06,
			0x10, 0x4c, 0x9c, 0xeb, 0x35, 0xb4, 0x5c, 0xef, 0x76,
			0x00, 0x14, 0x90, 0x46, 0x71, 0x01, 0x4a, 0x19, 0x3f,
			0x40, 0xc1, 0x5f, 0xc2, 0x44,
		},
		.okm = {
			0xb1, 0x1e, 0x39, 0x8d, 0xc8, 0x03, 0x27, 0xa1, 0xc8,
			0xe7, 0xf7, 0x8c, 0x59, 0x6a, 0x49, 0x34, 0x4f, 0x01,
			0x2e, 0xda, 0x2d, 0x4e, 0xfa, 0xd8, 0xa0, 0x50, 0xcc,
			0x4c, 0x19, 0xaf, 0xa9, 0x7c, 0x59, 0x04, 0x5a, 0x99,
			0xca, 0xc7, 0x82, 0x72, 0x71, 0xcb, 0x41, 0xc6, 0x5e,
			0x59, 0x0e, 0x09, 0xda, 0x32, 0x75, 0x60, 0x0c, 0x2f,
			0x09, 0xb8, 0x36, 0x77, 0x93, 0xa9, 0xac, 0xa3, 0xdb,
			0x71, 0xcc, 0x30, 0xc5, 0x81, 0x79, 0xec, 0x3e, 0x87,
			0xc1, 0x4c, 0x01, 0xd5, 0xc1, 0xf3, 0x43, 0x4f, 0x1d,
			0x87,
		},
	};

	/* https://datatracker.ietf.org/doc/html/rfc5869#appendix-A.3 */
	const HkdfTestVector test_vector3 = {
		.prk = {
			0x19, 0xef, 0x24, 0xa3, 0x2c, 0x71, 0x7b, 0x16, 0x7f,
			0x33, 0xa9, 0x1d, 0x6f, 0x64, 0x8b, 0xdf, 0x96, 0x59,
			0x67, 0x76, 0xaf, 0xdb, 0x63, 0x77, 0xac, 0x43, 0x4c,
			0x1c, 0x29, 0x3c, 0xcb, 0x04,
		},
		.okm = {
			0x8d, 0xa4, 0xe7, 0x75, 0xa5, 0x63, 0xc1, 0x8f, 0x71,
			0x5f, 0x80, 0x2a, 0x06, 0x3c, 0x5a, 0x31, 0xb8, 0xa1,
			0x1f, 0x5c, 0x5e, 0xe1, 0x87, 0x9e, 0xc3, 0x45, 0x4e,
			0x5f, 0x3c, 0x73, 0x8d, 0x2d, 0x9d, 0x20, 0x13, 0x95,
			0xfa, 0xa4, 0xb6, 0x1a, 0x96, 0xc8,
		}
	};

	std::array<uint8_t, SHA256_DIGEST_SIZE> unused_output{};

	const std::array test_vectors = { test_vector1, test_vector2,
					  test_vector3 };

	for (const auto &test_vector : test_vectors) {
		const auto &expected_okm = test_vector.okm;
		std::vector<uint8_t> actual_okm(expected_okm.size());

		TEST_ASSERT(hkdf_expand(actual_okm.data(), actual_okm.size(),
					test_vector.prk.data(),
					test_vector.prk.size(),
					test_vector.info.data(),
					test_vector.info.size()) == EC_SUCCESS);
		TEST_ASSERT_ARRAY_EQ(expected_okm.data(), actual_okm.data(),
				     expected_okm.size());
	}

	TEST_ASSERT(hkdf_expand(nullptr, unused_output.size(),
				test_vector1.prk.data(),
				test_vector1.prk.size(),
				test_vector1.info.data(),
				test_vector1.info.size()) == EC_ERROR_INVAL);

	TEST_ASSERT(hkdf_expand(unused_output.data(), unused_output.size(),
				nullptr, test_vector1.prk.size(),
				test_vector1.info.data(),
				test_vector1.info.size()) == EC_ERROR_INVAL);
	TEST_ASSERT(hkdf_expand(unused_output.data(), unused_output.size(),
				test_vector1.prk.data(),
				test_vector1.prk.size(), nullptr,
				test_vector1.info.size()) == EC_ERROR_INVAL);
	/* Info size too long. */
	TEST_ASSERT(
		hkdf_expand(unused_output.data(), unused_output.size(),
			    test_vector1.prk.data(), test_vector1.prk.size(),
			    test_vector1.info.data(), 1024) == EC_ERROR_INVAL);
	/* OKM size too big. */
	TEST_ASSERT(hkdf_expand(unused_output.data(), 256 * SHA256_DIGEST_SIZE,
				test_vector1.prk.data(),
				test_vector1.prk.size(),
				test_vector1.info.data(),
				test_vector1.info.size()) == EC_ERROR_INVAL);
	return EC_SUCCESS;
}

test_static ec_error_list test_aes_128_gcm_encrypt_in_place()
{
	constexpr std::array<uint8_t, SBP_ENC_KEY_LEN> key = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	};
	std::array<uint8_t, 16> plaintext = { 0x00, 0x00, 0x00, 0x00,
					      0x00, 0x00, 0x00, 0x00,
					      0x00, 0x00, 0x00, 0x00,
					      0x00, 0x00, 0x00, 0x00 };
	constexpr std::array<uint8_t, 16> expected_ciphertext = {
		0x9b, 0xde, 0x09, 0x85, 0x27, 0x8c, 0x70, 0x89,
		0x54, 0x28, 0xcc, 0x4e, 0x7a, 0x36, 0xb1, 0x2d,
	};
	constexpr std::array<uint8_t, FP_CONTEXT_NONCE_BYTES> nonce = {
		0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06,
		0x05, 0x04, 0x03, 0x02, 0x01, 0x00
	};
	std::array<uint8_t, FP_CONTEXT_TAG_BYTES> tag{};
	constexpr std::array<uint8_t, FP_CONTEXT_TAG_BYTES> expected_tag = {
		0x85, 0x6e, 0xd2, 0x04, 0x1f, 0xe0, 0x8f, 0x0b,
		0xa1, 0xab, 0x8f, 0xb3, 0x70, 0x75, 0xab, 0x48,
	};

	ec_error_list ret =
		aes_128_gcm_encrypt(key, plaintext, plaintext, nonce, tag);
	TEST_EQ(ret, EC_SUCCESS, "%d");
	TEST_ASSERT_ARRAY_EQ(plaintext.data(), expected_ciphertext.data(),
			     plaintext.size());
	TEST_ASSERT_ARRAY_EQ(tag.data(), expected_tag.data(), tag.size());

	return EC_SUCCESS;
}

test_static ec_error_list test_aes_128_gcm_decrypt_in_place()
{
	constexpr std::array<uint8_t, SBP_ENC_KEY_LEN> key = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	};
	/* Using the same values as from test_aes_gcm_encrypt_in_place means we
	 * should get back the original plaintext from that function.
	 */
	constexpr std::array<uint8_t, 16> expected_plaintext = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	std::array<uint8_t, 16> ciphertext = {
		0x9b, 0xde, 0x09, 0x85, 0x27, 0x8c, 0x70, 0x89,
		0x54, 0x28, 0xcc, 0x4e, 0x7a, 0x36, 0xb1, 0x2d,
	};
	constexpr std::array<uint8_t, FP_CONTEXT_NONCE_BYTES> nonce = {
		0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06,
		0x05, 0x04, 0x03, 0x02, 0x01, 0x00
	};
	constexpr std::array<uint8_t, FP_CONTEXT_TAG_BYTES> tag = {
		0x85, 0x6e, 0xd2, 0x04, 0x1f, 0xe0, 0x8f, 0x0b,
		0xa1, 0xab, 0x8f, 0xb3, 0x70, 0x75, 0xab, 0x48,
	};

	ec_error_list ret =
		aes_128_gcm_decrypt(key, ciphertext, ciphertext, nonce, tag);
	;
	TEST_EQ(ret, EC_SUCCESS, "%d");
	TEST_ASSERT_ARRAY_EQ(ciphertext.data(), expected_plaintext.data(),
			     ciphertext.size());

	return EC_SUCCESS;
}

test_static ec_error_list test_aes_128_gcm_encrypt_invalid_nonce_size()
{
	constexpr std::array<uint8_t, SBP_ENC_KEY_LEN> key{};
	std::array<uint8_t, 16> text{};
	std::array<uint8_t, FP_CONTEXT_TAG_BYTES> tag{};

	/* Use an invalid nonce size. */
	constexpr std::array<uint8_t, FP_CONTEXT_NONCE_BYTES - 1> nonce{};

	ec_error_list ret = aes_128_gcm_encrypt(key, text, text, nonce, tag);
	TEST_EQ(ret, EC_ERROR_INVAL, "%d");

	return EC_SUCCESS;
}

test_static ec_error_list test_aes_128_gcm_decrypt_invalid_nonce_size()
{
	constexpr std::array<uint8_t, SBP_ENC_KEY_LEN> key{};
	std::array<uint8_t, 16> text{};
	constexpr std::array<uint8_t, FP_CONTEXT_TAG_BYTES> tag{};

	/* Use an invalid nonce size. */
	constexpr std::array<uint8_t, FP_CONTEXT_NONCE_BYTES - 1> nonce{};

	ec_error_list ret = aes_128_gcm_decrypt(key, text, text, nonce, tag);
	TEST_EQ(ret, EC_ERROR_INVAL, "%d");
	return EC_SUCCESS;
}

test_static ec_error_list test_aes_128_gcm_encrypt_invalid_key_size()
{
	std::array<uint8_t, 16> text{};
	std::array<uint8_t, FP_CONTEXT_TAG_BYTES> tag{};
	constexpr std::array<uint8_t, FP_CONTEXT_NONCE_BYTES> nonce{};

	/* Use an invalid key size. Key must be exactly 128 bits. */
	constexpr std::array<uint8_t, SBP_ENC_KEY_LEN - 1> key{};

	ec_error_list ret = aes_128_gcm_encrypt(key, text, text, nonce, tag);
	TEST_EQ(ret, EC_ERROR_UNKNOWN, "%d");

	return EC_SUCCESS;
}

test_static ec_error_list test_aes_128_gcm_decrypt_invalid_key_size()
{
	std::array<uint8_t, 16> text{};
	constexpr std::array<uint8_t, FP_CONTEXT_TAG_BYTES> tag{};
	constexpr std::array<uint8_t, FP_CONTEXT_NONCE_BYTES> nonce{};

	/* Use an invalid key size. Key must be exactly 128 bits. */
	constexpr std::array<uint8_t, SBP_ENC_KEY_LEN - 1> key{};

	ec_error_list ret = aes_128_gcm_decrypt(key, text, text, nonce, tag);
	TEST_EQ(ret, EC_ERROR_UNKNOWN, "%d");
	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	RUN_TEST(test_aes_128_gcm_encrypt_in_place);
	RUN_TEST(test_aes_128_gcm_decrypt_in_place);
	RUN_TEST(test_aes_128_gcm_encrypt_invalid_nonce_size);
	RUN_TEST(test_aes_128_gcm_decrypt_invalid_nonce_size);
	RUN_TEST(test_aes_128_gcm_encrypt_invalid_key_size);
	RUN_TEST(test_aes_128_gcm_decrypt_invalid_key_size);
	RUN_TEST(test_hkdf_expand);

	test_print_result();
}
