import type { AstroIntegration, DataEntryType, HookParameters } from 'astro';

export default function rawFiles(...exts: [string]): AstroIntegration {
	return {
		name: 'raw-files',
		hooks: {
			'astro:config:setup': async (params) => {
				const {
					logger,
					addDataEntryType,
				} = params as HookParameters<'astro:config:setup'> & {
					addDataEntryType: (entry: DataEntryType) => void;
				}
				logger.info(`extensions: ${exts}`)

				addDataEntryType({
					// extensions: exts,
					extensions: ["."],
					getEntryInfo({ contents }) {
						return {
							data: {
								contents,
							},
						}
					}
				})
			},
		},
	};
}
