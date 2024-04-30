import { /*z,*/ defineCollection } from "astro:content";

export const collections = {
	talvos: defineCollection({
		type: 'data',
		// TODO `schema: z.string()` ?
		// schema: z.string(),
		// schema: z.object({
		// 	"raw": z.string(),
		// }),
	}),
};
