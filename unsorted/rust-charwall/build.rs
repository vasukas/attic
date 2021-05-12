use bindgen;
use std::env;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());

    Command::new("git")
        .current_dir(&out_path)
        .args(&["clone", "https://github.com/vasukas/xglwall.git"])
        .output()
        .expect("failed to clone");

    let cpp_path = out_path.join("xglwall");

    Command::new("sh")
        .current_dir(&cpp_path)
        .args(&["sobuild.sh"])
        .output()
        .expect("failed to build");

    bindgen::Builder::default()
        .header(cpp_path.join("xglwall.h").to_string_lossy())
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .generate()
        .expect("Unable to generate bindings")
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");

    println!(
        "cargo:rustc-link-search=native={}",
        (cpp_path.join("build")).to_str().unwrap()
    );
    println!("cargo:rustc-link-lib=dylib=xglwall");
}
