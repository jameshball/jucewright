        juce::var screenshot (juce::DynamicObject& params)
        {
            if (root == nullptr)
                return error ("no_root", "No root component is attached.");

            const auto ref = getString (params, "ref", {});
            juce::Component* target = nullptr;
            auto targetName = getString (params, "target", {});

            if (hasTargetSelector (params))
            {
                auto resolution = resolveTarget (params, true, true);

                if (!resolution.error.isVoid())
                    return resolution.error;

                target = resolution.component;
            }

            if (ref.isNotEmpty() && target == nullptr)
                return error ("stale_ref", "Run snapshot again.");

            if (!hasTargetSelector (params) && ref.isEmpty() && targetName.isNotEmpty() && targetName != "root")
            {
                target = automationWindowForId (targetName);

                if (target == nullptr)
                    return error ("window_not_found", "No automation window matched target: " + targetName);
            }

            if (!hasTargetSelector (params) && ref.isEmpty() && (targetName == "root" || target == nullptr))
                target = root.getComponent();

            if (target == nullptr)
                return error ("stale_ref", "Run snapshot again.");

            const auto menuName = getString (params, "menu", {});

            if (menuName.isNotEmpty())
            {
                auto* menuBar = findMenuBarComponent (*target);

                if (menuBar == nullptr || menuBar->getModel() == nullptr)
                    return error ("menu_bar_not_found", "The screenshot target does not contain a menu bar.");

                const auto menuNames = menuBar->getModel()->getMenuBarNames();
                const auto menuIndex = menuNames.indexOf (menuName, true);

                if (menuIndex < 0)
                    return error ("menu_not_found", "Menu bar item not found: " + menuName);

                menuBar->showMenu (menuIndex);
            }

            auto area = target->getLocalBounds();

            if (!params.getProperty ("clipW").isVoid() || !params.getProperty ("clipH").isVoid())
            {
                area = { getInt (params, "clipX", 0),
                         getInt (params, "clipY", 0),
                         getInt (params, "clipW", area.getWidth()),
                         getInt (params, "clipH", area.getHeight()) };
                area = area.getIntersection (target->getLocalBounds());
            }

            if (area.isEmpty())
                return error ("screenshot_failed", "Screenshot clip is empty.");

            const auto scale = (float) getDouble (params, "scale", 1.0);

            if (scale <= 0.0f || scale > 4.0f)
                return error ("invalid_screenshot_scale", "Screenshot scale must be greater than 0 and no more than 4.");

            const auto source = getString (params, "source", "auto");

            if (source != "auto" && source != "component" && source != "native")
                return error ("invalid_screenshot_source", "Screenshot source must be auto, component, or native.");

            juce::String nativeFailure;
            auto image = juce::Image();
            auto capturedAllOpenGL = true;
            auto capturedTransientWindows = 0;
            auto sourceUsed = source == "auto" ? juce::String ("component") : source;

            if (source == "native")
            {
                image = createNativeScreenshot (*target, area, scale, nativeFailure);

                if (image.isNull())
                    return error ("screenshot_failed", nativeFailure.isNotEmpty() ? nativeFailure
                                                                                  : juce::String ("Could not create native screenshot."));
            }
            else
            {
                image = createComponentScreenshot (*target, area, scale, capturedAllOpenGL);
            }

            if (source == "auto" && (image.isNull() || ! capturedAllOpenGL))
            {
                auto nativeImage = createNativeScreenshot (*target, area, scale, nativeFailure);

                if (! nativeImage.isNull())
                {
                    image = nativeImage;
                    sourceUsed = "native";
                }
            }

            if (image.isNull())
                return error ("screenshot_failed", nativeFailure.isNotEmpty() ? nativeFailure
                                                                              : juce::String ("Could not create screenshot."));

            capturedTransientWindows = compositeTransientWindows (*target, area, image);

            juce::MemoryBlock pngBytes;
            juce::MemoryOutputStream stream (pngBytes, false);
            juce::PNGImageFormat png;

            if (!png.writeImageToStream (image, stream))
                return error ("screenshot_failed", "Could not encode PNG.");

            const auto filePath = getString (params, "file", {});
            juce::String absolutePath;

            if (filePath.isNotEmpty())
            {
                auto fileOrError = writableArtifactFile (filePath);

                if (auto* errorObject = fileOrError.getDynamicObject())
                    if (errorObject->getProperty ("__error").isString())
                        return fileOrError;

                auto file = juce::File (fileOrError.toString());

                file.getParentDirectory().createDirectory();

                if (!file.replaceWithData (pngBytes.getData(), pngBytes.getSize()))
                    return error ("screenshot_failed", "Could not write PNG file: " + file.getFullPathName());

                absolutePath = file.getFullPathName();
            }

            auto result = object ({ { "mimeType", "image/png" },
                                    { "width", image.getWidth() },
                                    { "height", image.getHeight() },
                                    { "source", sourceUsed },
                                    { "capturedAllOpenGL", capturedAllOpenGL },
                                    { "capturedTransientWindows", capturedTransientWindows },
                                    { "file", absolutePath } });

            if ((bool) params.getProperty ("includeBase64"))
                result.getDynamicObject()->setProperty ("base64", juce::Base64::toBase64 (pngBytes.getData(), pngBytes.getSize()));

            return result;
        }

        static juce::MenuBarComponent* findMenuBarComponent (juce::Component& component)
        {
            if (auto* menuBar = dynamic_cast<juce::MenuBarComponent*> (&component))
                return menuBar;

            for (auto* child : component.getChildren())
            {
                if (child != nullptr)
                    if (auto* menuBar = findMenuBarComponent (*child))
                        return menuBar;
            }

            return nullptr;
        }

        juce::Image createComponentScreenshot (juce::Component& target,
                                               juce::Rectangle<int> area,
                                               float scale,
                                               bool& capturedAllOpenGL) const
        {
            auto image = target.createComponentSnapshot (area, false, scale);
            capturedAllOpenGL = true;

            if (!image.isNull())
                capturedAllOpenGL = compositeOpenGLComponents (target, area, scale, image);

            return image;
        }

        int compositeTransientWindows (juce::Component& target,
                                       juce::Rectangle<int> area,
                                       juce::Image& image) const
        {
            if (image.isNull() || area.isEmpty())
                return 0;

            auto* targetTopLevel = target.getTopLevelComponent();

            if (targetTopLevel == nullptr)
                targetTopLevel = &target;

            const auto globalArea = target.localAreaToGlobal (area);
            const auto scaleX = (float) image.getWidth() / (float) area.getWidth();
            const auto scaleY = (float) image.getHeight() / (float) area.getHeight();
            auto& desktop = juce::Desktop::getInstance();
            juce::Graphics graphics (image);
            auto captured = 0;

            for (int i = 0; i < desktop.getNumComponents(); ++i)
            {
                auto* component = desktop.getComponent (i);

                if (component == nullptr
                    || component == targetTopLevel
                    || !component->isShowing()
                    || !isTransientScreenshotComponent (*component))
                {
                    continue;
                }

                const auto screenBounds = component->getScreenBounds();

                if (!screenBounds.intersects (globalArea))
                    continue;

                auto transientImage = component->createComponentSnapshot (component->getLocalBounds(), false, scaleX);

                if (transientImage.isNull())
                    continue;

                const auto drawBounds = juce::Rectangle<float> {
                    (float) (screenBounds.getX() - globalArea.getX()) * scaleX,
                    (float) (screenBounds.getY() - globalArea.getY()) * scaleY,
                    (float) screenBounds.getWidth() * scaleX,
                    (float) screenBounds.getHeight() * scaleY
                };
                graphics.drawImage (transientImage, drawBounds);
                ++captured;
            }

            return captured;
        }

        static bool isTransientScreenshotComponent (juce::Component& component)
        {
            auto* handler = component.getAccessibilityHandler();

            if (handler != nullptr)
            {
                const auto role = handler->getRole();

                if (role == juce::AccessibilityRole::popupMenu || role == juce::AccessibilityRole::tooltip)
                    return true;
            }

            return type (component).containsIgnoreCase ("PopupMenu");
        }

        bool compositeOpenGLComponents (juce::Component& target,
                                        juce::Rectangle<int> area,
                                        float scale,
                                        juce::Image& image) const
        {
            juce::Array<juce::Component*> openGLComponents;
            collectOpenGLComponents (target, openGLComponents);

            if (openGLComponents.isEmpty())
                return true;

            juce::Graphics g (image);
            auto capturedAllOpenGL = true;

            for (auto* component : openGLComponents)
            {
                if (component == nullptr || !component->isShowing())
                    continue;

                const auto componentBoundsInTarget = target.getLocalArea (component, component->getLocalBounds());
                const auto visibleArea = componentBoundsInTarget.getIntersection (area);

                if (visibleArea.isEmpty())
                    continue;

                auto openGLImage = captureOpenGLFramebuffer (*component);

                if (openGLImage.isNull())
                {
                    capturedAllOpenGL = false;
                    continue;
                }

                const auto drawBounds = juce::Rectangle<float> ((float) (componentBoundsInTarget.getX() - area.getX()) * scale,
                                                                (float) (componentBoundsInTarget.getY() - area.getY()) * scale,
                                                                (float) componentBoundsInTarget.getWidth() * scale,
                                                                (float) componentBoundsInTarget.getHeight() * scale);

                const auto clip = juce::Rectangle<float> ((float) (visibleArea.getX() - area.getX()) * scale,
                                                          (float) (visibleArea.getY() - area.getY()) * scale,
                                                          (float) visibleArea.getWidth() * scale,
                                                          (float) visibleArea.getHeight() * scale)
                                      .getSmallestIntegerContainer()
                                      .getIntersection (image.getBounds());

                juce::Graphics::ScopedSaveState saveState (g);
                g.reduceClipRegion (clip);
                g.drawImage (openGLImage, drawBounds);
            }

            return capturedAllOpenGL;
        }

        static void collectOpenGLComponents (juce::Component& component, juce::Array<juce::Component*>& components)
        {
           #if JUCE_MODULE_AVAILABLE_juce_opengl
            if (juce::OpenGLContext::getContextAttachedTo (component) != nullptr)
                components.add (&component);
           #endif

            for (auto* child : component.getChildren())
                if (child != nullptr)
                    collectOpenGLComponents (*child, components);
        }

        static juce::Image captureOpenGLFramebuffer (juce::Component& component)
        {
           #if JUCE_MODULE_AVAILABLE_juce_opengl
            auto* context = juce::OpenGLContext::getContextAttachedTo (component);

            if (context == nullptr || !context->isAttached() || component.getWidth() <= 0 || component.getHeight() <= 0)
                return {};

            // The GL drawable can be Retina-scaled even when the component scale reports 1.0.
            const auto renderScale = juce::jmax (1.0, context->getRenderingScale());
            const auto width = juce::jmax (1, juce::roundToInt ((double) component.getWidth() * renderScale));
            const auto height = juce::jmax (1, juce::roundToInt ((double) component.getHeight() * renderScale));
            std::vector<juce::uint8> rgba ((size_t) width * (size_t) height * 4u);
            std::atomic<bool> succeeded { false };

            context->executeOnGLThread ([&] (juce::OpenGLContext& activeContext) {
                using namespace juce::gl;

                GLint previousPackAlignment = 4;
                glGetIntegerv (GL_PACK_ALIGNMENT, &previousPackAlignment);
                glPixelStorei (GL_PACK_ALIGNMENT, 1);

                GLint previousFramebuffer = 0;
                glGetIntegerv (GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
                glBindFramebuffer (GL_FRAMEBUFFER, activeContext.getFrameBufferID());

                GLint previousViewport[4] {};
                glGetIntegerv (GL_VIEWPORT, previousViewport);
                glViewport (0, 0, width, height);

                const auto previousScissorEnabled = glIsEnabled (GL_SCISSOR_TEST);
                glDisable (GL_SCISSOR_TEST);

               #if ! JUCE_OPENGL_ES
                GLint previousReadBuffer = GL_BACK;
                glGetIntegerv (GL_READ_BUFFER, &previousReadBuffer);
                GLint previousDrawBuffer = GL_BACK;
                glGetIntegerv (GL_DRAW_BUFFER, &previousDrawBuffer);

                const auto drawBuffer = activeContext.getFrameBufferID() == 0 ? (GLenum) GL_BACK : (GLenum) GL_COLOR_ATTACHMENT0;
                const auto readBuffer = drawBuffer;
                glDrawBuffer (drawBuffer);
                glReadBuffer (readBuffer);
               #endif

                // JUCE runs queued GL work before the next render callback, so render once here
                // before reading from the back buffer.
                if (auto* renderer = dynamic_cast<juce::OpenGLRenderer*> (&component))
                    renderer->renderOpenGL();

                glFinish();
                glReadPixels (0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
                const auto readError = glGetError();

               #if ! JUCE_OPENGL_ES
                glReadBuffer ((GLenum) previousReadBuffer);
                glDrawBuffer ((GLenum) previousDrawBuffer);
               #endif

                if (previousScissorEnabled)
                    glEnable (GL_SCISSOR_TEST);

                glViewport (previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
                glBindFramebuffer (GL_FRAMEBUFFER, (GLuint) previousFramebuffer);
                glPixelStorei (GL_PACK_ALIGNMENT, previousPackAlignment);
                succeeded.store (readError == GL_NO_ERROR);
            }, true);

            if (!succeeded.load())
                return {};

            juce::Image image (juce::Image::ARGB, width, height, true);

            for (int y = 0; y < height; ++y)
            {
                const auto sourceY = height - 1 - y;

                for (int x = 0; x < width; ++x)
                {
                    const auto offset = ((size_t) sourceY * (size_t) width + (size_t) x) * 4u;
                    image.setPixelAt (x, y, juce::Colour::fromRGBA (rgba[offset],
                                                                     rgba[offset + 1],
                                                                     rgba[offset + 2],
                                                                     255));
                }
            }

            juce::Image componentOverlay (juce::Image::ARGB, width, height, true);
            juce::Graphics overlayGraphics (componentOverlay);
            overlayGraphics.addTransform (juce::AffineTransform::scale ((float) width / (float) component.getWidth(),
                                                                         (float) height / (float) component.getHeight()));
            component.paintEntireComponent (overlayGraphics, true);

            juce::Graphics imageGraphics (image);
            imageGraphics.drawImageAt (componentOverlay, 0, 0);

            return image;
           #else
            juce::ignoreUnused (component);
            return {};
           #endif
        }

        juce::Image createNativeScreenshot (juce::Component& target,
                                            juce::Rectangle<int> area,
                                            float scale,
                                            juce::String& failure) const
        {
            return NativeServices::createNativeScreenshot (target, area, scale, failure);
        }

        juce::var writableArtifactFile (const juce::String& requestedPath) const
        {
            if (!options.allowFileWrite)
                return error ("file_write_disabled", "Automation file output is disabled for this session.");

            const auto hasArtifactRoot = options.artifactRoot.getFullPathName().isNotEmpty();
            juce::File file;

            if (hasArtifactRoot)
            {
                auto rootDirectory = options.artifactRoot;

                if (!juce::File::isAbsolutePath (requestedPath))
                    file = rootDirectory.getChildFile (requestedPath);
                else
                    file = juce::File (requestedPath);

                if (!file.isAChildOf (rootDirectory))
                    return error ("artifact_path_denied", "Automation file output must stay within the artifact root.");
            }
            else
            {
                if (!juce::File::isAbsolutePath (requestedPath))
                    return error ("invalid_file_path", "Automation file output requires an absolute path when no artifact root is configured.");

                file = juce::File (requestedPath);
            }

            return file.getFullPathName();
        }
