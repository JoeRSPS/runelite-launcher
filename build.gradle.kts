/*
 * Copyright (c) 2025, Adam <Adam@sigterm.info>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

plugins {
    java
    application
    id("com.gradleup.shadow") version "8.3.8"
}

repositories {
    mavenCentral()
    maven { url = uri("https://repo.runelite.net") }
}

group = "net.runelite"
version = "2.7.6-SNAPSHOT"
description = "Elyra Launcher"

dependencies {
    implementation(libs.slf4j.api)
    implementation(libs.logback.classic)
    implementation(libs.jopt.simple)
    implementation(libs.gson)
    implementation(libs.guava) {
        // compile time annotations for static analysis in Guava
        // https://github.com/google/guava/wiki/UseGuavaInYourBuild#what-about-guavas-own-dependencies
        exclude(group = "com.github.spotbugs", module = "spotbugs-annotations")
        exclude(group = "com.google.code.findbugs", module = "jsr305")
        exclude(group = "com.google.errorprone", module = "error_prone_annotations")
        exclude(group = "com.google.j2objc", module = "j2objc-annotations")
        exclude(group = "org.codehaus.mojo", module = "animal-sniffer-annotations")
    }
    implementation(libs.runelite.archive.patcher.applier)
    compileOnly(libs.spotbugs.annotations)
    compileOnly(libs.lombok)
    annotationProcessor(libs.lombok)
    testImplementation(libs.junit)
}

tasks.withType<JavaCompile> {
    options.encoding = "UTF-8"
    options.release.set(11)
}

tasks.withType<AbstractArchiveTask>().configureEach {
    isPreserveFileTimestamps = false
    isReproducibleFileOrder = true
}

sourceSets.create("java8") {
    java.srcDirs("src/main/java8")
}

tasks.jar {
    from(sourceSets["java8"].output)
    duplicatesStrategy = DuplicatesStrategy.WARN
}

tasks.getByName<JavaCompile>("compileJava8Java") {
    options.release.unset()
    sourceCompatibility = "1.8"
    targetCompatibility = "1.8"
}

tasks {
    processResources {
        filesMatching("**/*.properties") {
            val props = if (project.findProperty("RUNELITE_BUILD") as? String == "runelite")
                arrayOf(
                    "runelite_net" to "elyra-ps.com",
                    "runelite_128" to "runelite_128.png",
                    "runelite_splash" to "runelite_splash.png"
                )
            else arrayOf(
                "runelite_net" to "",
                "runelite_128" to "",
                "runelite_splash" to ""
            )
            expand(
                "project" to project,
                *props
            )
        }
    }
}

tasks.register<Copy>("filterAppimage") {
    from("appimage/runelite.desktop")
    into("build/filtered-resources")
    expand("project" to project)
}

tasks.register<Copy>("filterInnosetup") {
    from("innosetup") {
        include("*.iss")
    }
    into("build/filtered-resources")
    expand("project" to project) {
        escapeBackslash = true
    }
}

tasks.register<Copy>("copyInstallerScripts") {
    from("innosetup") {
        include("*.pas")
    }
    // not really filtered, but need to be put next to the filtered installer scripts so they can pick them up
    into("build/filtered-resources")
}

tasks.register<Copy>("filterOsx") {
    from("osx/Info.plist")
    into("build/filtered-resources")
    expand("project" to project)
}

val mainClassName = "net.runelite.launcher.Launcher"

application {
    mainClass.set(mainClassName)
}

tasks.shadowJar {
    from(sourceSets.main.get().output)
    from(sourceSets.getByName("java8").output)
    minimize {
        exclude(dependency("ch.qos.logback:.*:.*"))
    }
    archiveFileName.set(project.findProperty("finalName") as String + ".jar")
    manifest {
        attributes("Main-Class" to mainClassName)
    }
}

tasks.named("build") {
    dependsOn("filterAppimage", "filterInnosetup", "copyInstallerScripts", "filterOsx", tasks.shadowJar)
}
