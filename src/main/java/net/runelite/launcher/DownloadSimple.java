package net.runelite.launcher;

import lombok.extern.slf4j.Slf4j;
import net.runelite.launcher.beans.Artifact;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.List;

/**
 * @author Jire
 */
@Slf4j
enum DownloadSimple {

    ;

    static void download(
            final List<Artifact> artifacts
    ) throws IOException {
        final double START_PROGRESS = .15;

        long netSize = Long.MIN_VALUE;
        SplashScreen.stage(START_PROGRESS, "Downloading", "");

        for (final Artifact artifact : artifacts) {
            final String artifactName = artifact.getName();
            final String artifactNetPath = artifact.getPath();

            final File artifactFile = new File(Launcher.REPO_DIR, artifactName);
            final Path artifactFilePath = artifactFile.toPath();

            final HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create(artifactNetPath))
                    .header("User-Agent", Launcher.USER_AGENT)
                    .GET()
                    .build();

            final HttpResponse<InputStream> response;
            try {
                response = Launcher.httpClient.send(request, HttpResponse.BodyHandlers.ofInputStream());
            } catch (final InterruptedException ex) {
                throw new IOException(ex);
            }

            if (response.statusCode() != 200) {
                throw new IOException("Unable to download " + artifactNetPath + " (status code " + response.statusCode() + ")");
            }

            boolean upToDate = false;

            long diskSize = Long.MIN_VALUE;

            if (Files.exists(artifactFilePath)) {
                try {
                    diskSize = Files.size(artifactFilePath);
                } catch (final Exception ex) {
                    log.error("Error getting size of file \"" + artifactFilePath + "\"", ex);
                }
            }

            try {
                final List<String> contentLengthHeaders = response
                        .headers()
                        .allValues("Content-Length");
                if (contentLengthHeaders.isEmpty()) {
                    log.warn("Content-Length header not found for artifact path \"{}\"", artifactNetPath);
                } else {
                    final String contentLength = contentLengthHeaders.get(0);
                    log.debug("Artifact path \"{}\" content length file size: \"{}\"",
                            artifactNetPath, contentLength);

                    netSize = Long.parseLong(contentLength);

                    if (diskSize == netSize) {
                        upToDate = true;
                    }

                    log.debug("Artifact \"{}\" - disk size: {}, net size: {}, up-to-date: {}",
                            artifactName,
                            diskSize, netSize,
                            upToDate);
                }
            } catch (final Exception ex) {
                log.error("Error getting content length header for artifact path \"" + artifactNetPath + "\"", ex);
            }

            if (upToDate) {
                log.debug("Artifact \"{}\" - up to date", artifactName);
                continue;
            }

            log.debug("Downloading \"{}\"...", artifactName);

            long downloaded = 0;

            try (final InputStream in = response.body()) {
                try (final FileChannel fileChannel = FileChannel.open(
                        artifactFilePath,
                        StandardOpenOption.CREATE,
                        StandardOpenOption.WRITE)
                ) {
                    final ByteBuffer buffer = ByteBuffer.allocateDirect(1024 * 1024); // 1 MiB buffer
                    final byte[] bytes = new byte[buffer.capacity()];

                    int bytesRead;
                    while ((bytesRead = in.read(bytes)) != -1) {
                        buffer.clear();
                        buffer.put(bytes, 0, bytesRead);
                        buffer.flip();
                        while (buffer.hasRemaining()) {
                            final int written = fileChannel.write(buffer);
                            downloaded += written;

                            SplashScreen.stage(START_PROGRESS, .80, null, artifactName,
                                    downloaded, netSize, true);
                        }
                    }
                }
            }
        }
    }

}
