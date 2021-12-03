// swift-tools-version:5.5
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

var cxxSettings: [CXXSetting] = [
    .headerSearchPath("."),
    .headerSearchPath("include"),
    .define("REALM_SPM", to: "1"),
    .define("REALM_PLATFORM_APPLE", to: "1"),
    .define("REALM_ENABLE_SYNC", to: "1"),
//    .define("REALM_COCOA_VERSION", to: "@\"\(cocoaVersionStr)\""),
    .define("REALM_VERSION", to: "11.6.1"),

    .define("REALM_DEBUG", .when(configuration: .debug)),
    .define("REALM_NO_CONFIG"),
    .define("REALM_INSTALL_LIBEXECDIR", to: ""),
    .define("REALM_ENABLE_ASSERTIONS", to: "1"),
    .define("REALM_ENABLE_ENCRYPTION", to: "1"),

    .define("REALM_VERSION_MAJOR", to: "11"),
    .define("REALM_VERSION_MINOR", to: "6"),
    .define("REALM_VERSION_PATCH", to: "1"),
    .define("REALM_VERSION_EXTRA", to: "\"\""),
    .define("REALM_VERSION_STRING", to: "\"11.6.1\""),
]

#if swift(>=5.5)
cxxSettings.append(.define("REALM_HAVE_SECURE_TRANSPORT", to: "1", .when(platforms: [.macOS, .macCatalyst, .iOS, .tvOS, .watchOS])))
#else
cxxSettings.append(.define("REALM_HAVE_SECURE_TRANSPORT", to: "1", .when(platforms: [.macOS, .iOS, .tvOS, .watchOS])))
#endif

let testCxxSettings: [CXXSetting] = cxxSettings + [
    // Command-line `swift build` resolves header search paths
    // relative to the package root, while Xcode resolves them
    // relative to the target root, so we need both.
    .headerSearchPath("Realm"),
    .headerSearchPath("../"),
    .headerSearchPath("../../Sources/realm-cpp-sdk"),
]

let package = Package(
    name: "realm-cpp-sdk",
    platforms: [
        .macOS(.v10_15),
        .iOS(.v11),
        .tvOS(.v9),
        .watchOS(.v2)
    ],
    products: [
        // Products define the executables and libraries a package produces, and make them visible to other packages.
        .library(
            name: "realm-cpp-sdk",
            targets: ["realm-cpp-sdk"]),
    ],
    dependencies: [
        // Dependencies declare other packages that this package depends on.
         .package(url: "https://github.com/realm/realm-core", from: "11.6.1"),
    ],
    targets: [
        // Targets are the basic building blocks of a package. A target can define a module or a test suite.
        // Targets can depend on other targets in this package, and on products in packages this package depends on.
        .target(
            name: "realm-cpp-sdk",
            dependencies: [.product(name: "RealmCore", package: "realm-core")],
            publicHeadersPath: "include",
            cxxSettings: cxxSettings,
            linkerSettings: [
                .linkedFramework("Foundation", .when(platforms: [.macOS, .iOS, .tvOS, .watchOS])),
                .linkedFramework("Security", .when(platforms: [.macOS, .iOS, .tvOS, .watchOS])),
            ]),
        .testTarget(
            name: "realm-cpp-sdkTests",
            dependencies: ["realm-cpp-sdk"],
            cxxSettings: cxxSettings + [
                .headerSearchPath("../../Sources/realm-cpp-sdk/include")
            ]),
    ],
    cxxLanguageStandard: .cxx20
)
