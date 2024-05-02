use object::{Object, ObjectSection};
use std::process;
use std::{borrow, error, fs};
use typed_arena::Arena;

fn main() {
    // via https://github.com/gimli-rs/gimli/blob/c49f64df0920f89c20eda47f76ff3d3655ad0326/crates/examples/src/bin/dwarfdump.rs#L320
    let arena_mmap = Arena::new();
    let load_file = |path| -> Result<object::File, String> {
        let file = fs::File::open(&path)
            .map_err(|err| format!("Failed to open file '{}': {}", path, err))?;
        let mmap = unsafe { memmap2::Mmap::map(&file) }
            .map_err(|err| format!("Failed to map file '{}': {}", path, err))?;
        let mmap_ref = arena_mmap.alloc(mmap);

        object::File::parse(&**mmap_ref)
            .map_err(|err| format!("Failed to parse file '{}': {}", path, err))
    };

    let path = "../wasm/talvos-wasm.wasm";
    // let path = "../wasm/talvos-wasm.dwp";

    let obj = load_file(path.to_owned()).unwrap();
    if !obj.is_little_endian() {
        panic!("bad wasm file endianness")
    }

    // matches the discovery methodology of the chrome devtools:
    // cf. https://github.com/ChromeDevTools/devtools-frontend/blob/6f2433034d59c82daee92dca3737afec0762e466/extensions/cxx_debugging/src/MEMFSResourceLoader.ts#L18
    let dwp = load_file(format!("{path}.dwp"));
    // let dwp = load_file("../wasm/talvos-wasm.dwp".to_owned());

    if let Err(err) = dump_file(&obj, dwp.ok().as_ref(), gimli::RunTimeEndian::Little) {
        println!("fatal: {err:?}");
        process::exit(1)
    }

    // gimli::Dwarf::load(section)
    // println!("ok!");
}

// via https://github.com/gimli-rs/gimli/blob/33fe60c2e3374d836a3e845d4218630d76d68e6f/crates/examples/src/bin/simple.rs#L44
fn dump_file(
    object: &object::File,
    dwp_object: Option<&object::File>,
    endian: gimli::RunTimeEndian,
) -> Result<(), Box<dyn error::Error>> {
    // Load a section and return as `Cow<[u8]>`.
    fn load_section<'a>(
        object: &'a object::File,
        name: &str,
    ) -> Result<borrow::Cow<'a, [u8]>, Box<dyn error::Error>> {
        Ok(match object.section_by_name(name) {
            Some(section) => section.uncompressed_data()?,
            None => borrow::Cow::Borrowed(&[]),
        })
    }

    // Borrow a `Cow<[u8]>` to create an `EndianSlice`.
    let borrow_section = |section| gimli::EndianSlice::new(borrow::Cow::as_ref(section), endian);

    // Load all of the sections.
    let dwarf_sections = gimli::DwarfSections::load(|id| load_section(object, id.name()))?;
    let dwp_sections = dwp_object
        .map(|dwp_object| {
            gimli::DwarfPackageSections::load(|id| load_section(dwp_object, id.dwo_name().unwrap()))
        })
        .transpose()?;

    // Create `EndianSlice`s for all of the sections and do preliminary parsing.
    // Alternatively, we could have used `Dwarf::load` with an owned type such as `EndianRcSlice`.
    let dwarf = dwarf_sections.borrow(borrow_section);
    let dwp = dwp_sections
        .as_ref()
        .map(|dwp_sections| {
            dwp_sections.borrow(borrow_section, gimli::EndianSlice::new(&[], endian))
        })
        .transpose()?;

    // Iterate over the compilation units.
    let mut iter = dwarf.units();
    while let Some(header) = iter.next()? {
        let unit = dwarf.unit(header)?;
        let Some(comp_dir) = unit.comp_dir else {
            continue;
        };

        if comp_dir.starts_with(b"/emsdk/emscripten") {
            continue;
        }

        print!(
            "Unit at <.debug_info+0x{:x}>: ",
            header.offset().as_debug_info_offset().unwrap().0,
        );
        dump_unit(&unit)?;

        // Check for a DWO unit.
        let Some(dwp) = &dwp else { continue };
        let Some(dwo_id) = unit.dwo_id else { continue };
        print!("DWO Unit ID {:x}, name: ", dwo_id.0);
        let Some(dwo) = dwp.find_cu(dwo_id, &dwarf)? else {
            continue;
        };
        let Some(header) = dwo.units().next()? else {
            continue;
        };
        let unit = dwo.unit(header)?;
        dump_unit(&unit)?;
    }

    Ok(())
}

fn dump_unit(
    unit: &gimli::Unit<gimli::EndianSlice<gimli::RunTimeEndian>>,
) -> Result<(), gimli::Error> {
    if let Some(name) = unit.name {
        println!("{}", name.to_string()?)
    } else {
        println!("<no name>")
    }
    if let Some(dir) = unit.comp_dir {
        println!("    comp dir: {}", dir.to_string()?);
    }
    Ok(())
}
