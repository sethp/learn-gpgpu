import { defineConfig } from 'astro/config';
import rawFiles from "./astro/raw-files";

import starlight from "@astrojs/starlight";

// https://astro.build/config
export default defineConfig({
	site: 'https://sethp.github.io',
	base: 'learn-gpgpu',
	srcDir: ".",
	integrations: [rawFiles(".spvasm", ".tcf"), starlight({
		title: 'learn-gpgpu',
		disable404Route: true,
	})],
	server: _ => ({
		// headers: {
		// 	// required for shared buffers (used by emscripten to implement pthreads)
		// 	'access-control-allow-origin': 'same-origin',
		// 	'cross-origin-opener-policy': 'same-origin',
		// 	'cross-origin-embedder-policy': 'require-corp'
		// }
	}),
	vite: (() => {
		// see also: `--mount` list in Dockerfile.build for an include list mirror to these
		const viteExcludes = ['**/__snapshots__/**', '**/dist/**', '**/hack/**', '**/tmp/**', '**/tools/**', '**/wasm/*/**']
		return ({
			server: {
				watch: {
					ignored: viteExcludes,
				}
			},
			optimizeDeps: {
				// Otherwise, a big wall o' `Error:   Failed to scan for dependencies from entries:` results.
				// see: https://github.com/vitejs/vite/discussions/13306#discussioncomment-5973634
				// and: https://github.com/vitejs/vite/blob/e444375d34db1e1902f06ab223e51d2d63cd10de/packages/vite/src/node/optimizer/scan.ts#L183
				entries: ['**/**.html', ...viteExcludes.map(p => `!${p}`)],
			},
		});
	})()
});
