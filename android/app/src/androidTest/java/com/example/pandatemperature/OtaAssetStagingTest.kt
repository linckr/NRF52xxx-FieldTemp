package com.example.pandatemperature

import android.os.Bundle
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.example.pandatemperature.data.ota.OtaAuthKey
import com.example.pandatemperature.data.ota.OtaImageParser
import java.io.File
import java.security.MessageDigest
import java.util.zip.ZipFile
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Explicit local operator staging; never alters databases, settings or firmware. */
@RunWith(AndroidJUnit4::class)
class OtaAssetStagingTest {
    private fun sha(bytes: ByteArray) = MessageDigest.getInstance("SHA-256")
        .digest(bytes).joinToString("") { "%02x".format(it) }

    @Test fun stageOrCleanExplicitAssets() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val args = InstrumentationRegistry.getArguments()
        val action = args.getString("otaAssetsAction")
        assumeTrue("Explicit stage/cleanup required", action == "stage" || action == "cleanup")
        val expectedImage = requireNotNull(args.getString("expectedImageSha256"))
        require(expectedImage.matches(Regex("[0-9a-f]{64}")))
        val context = instrumentation.targetContext
        val directory = File(context.filesDir, "maintenance")
        val image = File(directory, "zephyr.signed.bin")
        val auth = File(directory, "auth.txt")
        if (action == "cleanup") {
            if (image.exists()) {
                assertEquals("Different staged image; refusing cleanup", expectedImage, sha(image.readBytes()))
                assertTrue(image.delete())
            }
            if (auth.exists()) assertTrue(auth.delete())
        } else {
            assertFalse("Refuse to replace an existing image", image.exists())
            assertFalse("Refuse to replace existing authorization material", auth.exists())
            val archive = File(requireNotNull(context.getExternalFilesDir(null)), "ota-assets.zip")
            assertEquals("Unexpected local archive", args.getString("assetsSha256"), sha(archive.readBytes()))
            val entries = linkedMapOf<String, ByteArray>()
            ZipFile(archive).use { zip ->
                val iterator = zip.entries()
                while (iterator.hasMoreElements()) {
                    val entry = iterator.nextElement()
                    require(!entry.isDirectory && entry.name in setOf("zephyr.signed.bin", "auth.txt"))
                    require(entry.name !in entries && entry.size in 1..163840)
                    entries[entry.name] = zip.getInputStream(entry).use { it.readBytes() }
                }
            }
            require(entries.keys == setOf("zephyr.signed.bin", "auth.txt"))
            val imageBytes = requireNotNull(entries["zephyr.signed.bin"])
            val authBytes = requireNotNull(entries["auth.txt"])
            assertEquals("Unexpected firmware", expectedImage, sha(imageBytes))
            OtaImageParser.parse(imageBytes) // Structural check; MCUboot verifies ECDSA-P256.
            require(authBytes.size <= 128 && OtaAuthKey.parse(authBytes.toString(Charsets.UTF_8).trim()) != null)
            assertTrue(directory.isDirectory || directory.mkdirs())
            try {
                image.writeBytes(imageBytes)
                auth.writeBytes(authBytes)
                assertEquals(expectedImage, sha(image.readBytes()))
                assertTrue(auth.readBytes().contentEquals(authBytes))
                assertTrue("Remove external authorization archive", archive.delete())
            } catch (failure: Throwable) {
                image.delete()
                auth.delete()
                throw failure
            }
        }
        instrumentation.sendStatus(0, Bundle().apply {
            putString("ota_assets_action", action)
            putString("image_sha256", expectedImage)
            putString("result", "PASS")
        })
    }
}
