import { defineConfig } from 'astro/config';

import rawFiles from "./astro/raw-files";

// https://astro.build/config
export default defineConfig({
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
				ignored: ['**/tmp/**'],
			},
		},
	}
});
