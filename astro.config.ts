import { defineConfig } from 'astro/config';

import rawFiles from "./astro/raw-files";

// https://astro.build/config
export default defineConfig({
	site: 'https://sethp.github.io',
	base: 'learn-gpgpu',
	srcDir: ".",
	integrations: [rawFiles(".spvasm", ".tcf")],
	server: (_) => ({
		// headers: {
		// 	// required for shared buffers (used by emscripten to implement pthreads)
		// 	'access-control-allow-origin': 'same-origin',
		// 	'cross-origin-opener-policy': 'same-origin',
		// 	'cross-origin-embedder-policy': 'require-corp'
		// }
	}),
	vite: {
		server: {
			watch: {
				// see also: `--mount` list in Dockerfile.build
				ignored: [
					'**/__snapshots__/**',
					'**/dist/**',
					'**/hack/**',
					'**/tmp/**',
					'**/tools/**',
					'**/wasm/*/**',
				],
			},
		},
	}
});
