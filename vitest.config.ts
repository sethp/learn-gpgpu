/// <reference types="vitest" />
import { getViteConfig } from 'astro/config';

const defaultExclude = ["node_modules", "dist", ".idea", ".git", ".cache"]

export default getViteConfig({
	test: {
		/* for example, use global to avoid globals imports (describe, test, expect): */
		// globals: true,
		// exclude
		exclude: [...defaultExclude, "tmp"],
		// watchExclude: ["**/tmp/**"],
	},
});
