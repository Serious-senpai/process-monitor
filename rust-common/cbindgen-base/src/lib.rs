use std::path::Path;

pub fn default<P>(lib_dir: P) -> cbindgen::Builder
where
    P: AsRef<Path>,
{
    cbindgen::Builder::new()
        .with_crate(lib_dir)
        .with_language(cbindgen::Language::Cxx)
        .with_tab_width(4)
        .with_braces(cbindgen::Braces::NextLine)
        .with_cpp_compat(true)
        .with_documentation(true)
        // .with_parse_deps(true)
        .with_pragma_once(true)
        .with_no_includes()
}
