        juce::var snapshot (juce::DynamicObject& params)
        {
            if (root == nullptr)
                return error ("no_root", "No root component is attached.");

            auto snapshotOpts = snapshotOptions (params);

            if (snapshotOpts.mode != "interesting" && snapshotOpts.mode != "full" && snapshotOpts.mode != "minimal")
                return error ("invalid_snapshot_mode", "Snapshot mode must be interesting, full, or minimal.");

            const auto ref = getString (params, "ref", {});
            auto* refTarget = ref.isNotEmpty() ? getTargetComponent (ref) : nullptr;

            if (ref.isNotEmpty() && refTarget == nullptr)
                return errorWithSuggestion ("stale_ref", "Run snapshot again.", "snapshot");

            auto locatorValue = params.getProperty ("locator");
            auto* locatorObject = locatorValue.getDynamicObject();

            if (ref.isNotEmpty() && locatorObject != nullptr)
                return error ("invalid_locator", "Pass either ref or locator, not both.");

            pruneRefs();
            ++generation;

            const auto maxDepth = getInt (params, "depth", 8);
            const auto format = getString (params, "format", "text");
            auto tree = scopedSnapshotTree (params, refTarget, juce::jmax (0, maxDepth));

            if (isError (tree))
                return tree;

            if (locatorObject != nullptr)
            {
                juce::Array<juce::var> matches;
                collectLocatorMatchNodes (matches, tree, *locatorObject, ! snapshotOpts.includeHidden);

                const auto nthValue = locatorObject->getProperty ("nth");

                if (!nthValue.isVoid())
                {
                    const auto nth = (int) nthValue;

                    if (juce::isPositiveAndBelow (nth, matches.size()))
                    {
                        auto selected = matches[nth];
                        matches.clear();
                        matches.add (selected);
                    }
                    else
                    {
                        matches.clear();
                    }
                }

                if (matches.isEmpty())
                    return locatorError ("locator_not_found", "Locator did not match any component.", *locatorObject, matches);

                if (matches.size() > 1)
                    return locatorError ("strict_mode_violation", "Locator matched " + juce::String (matches.size()) + " components.", *locatorObject, matches);

                tree = matches.getFirst();
            }

            auto outputTree = snapshotOpts.mode == "full" ? tree : interestingSnapshotTree (tree, snapshotOpts);

            if (outputTree.isVoid())
                outputTree = tree;

            juce::String text;
            appendTextSnapshot (text, outputTree, 0);
            const auto stateHash = calculateStateHash (outputTree);
            const auto since = getString (params, "since", {});

            if (since.isNotEmpty() && since == stateHash)
            {
                juce::Array<juce::var> changedRefs;
                auto unchanged = object ({ { "generation", generation },
                                           { "mode", snapshotOpts.mode },
                                           { "stateHash", stateHash },
                                           { "beforeStateHash", since },
                                           { "afterStateHash", stateHash },
                                           { "changed", false },
                                           { "changedRefs", changedRefs },
                                           { "changedSummary", "Snapshot has not changed." },
                                           { "suggestedNextSnapshotScope", suggestedSnapshotScope (params) },
                                           { "text", juce::String() } });

                if (format == "json")
                    return unchanged;

                return unchanged;
            }

            juce::Array<juce::var> changedRefs;
            collectRefs (outputTree, changedRefs);
            auto result = object ({ { "generation", generation },
                                    { "mode", snapshotOpts.mode },
                                    { "stateHash", stateHash },
                                    { "text", text } });

            auto* resultObject = result.getDynamicObject();

            if (resultObject != nullptr && since.isNotEmpty())
            {
                resultObject->setProperty ("beforeStateHash", since);
                resultObject->setProperty ("afterStateHash", stateHash);
                resultObject->setProperty ("changed", true);
                resultObject->setProperty ("changedRefs", changedRefs);
                resultObject->setProperty ("changedSummary", "Snapshot changed; inspect the returned context.");
                resultObject->setProperty ("suggestedNextSnapshotScope", suggestedSnapshotScope (params));
            }

            if (format == "json")
            {
                if (resultObject != nullptr)
                    resultObject->setProperty ("tree", outputTree);

                return result;
            }

            return result;
        }

        juce::var locator (juce::DynamicObject& params)
        {
            auto matchesOrError = resolveLocatorQuery (params, true, false);

            if (isError (matchesOrError))
                return matchesOrError;

            return matchesOrError;
        }

        juce::var count (juce::DynamicObject& params)
        {
            auto matchesOrError = resolveLocatorQuery (params, true, false, false);

            if (isError (matchesOrError))
                return matchesOrError;

            auto* result = matchesOrError.getDynamicObject();
            return object ({ { "count", result != nullptr ? (int) result->getProperty ("count") : 0 } });
        }

        juce::var describe (juce::DynamicObject& params)
        {
            auto resolution = resolveTarget (params, true, true);

            if (!resolution.error.isVoid())
                return resolution.error;

            auto snapshotOpts = snapshotOptions (params);
            snapshotOpts.includeHidden = getBool (params, "includeHidden", true);
            snapshotOpts.maxChildrenPerContainer = getInt (params, "maxChildrenPerContainer", 12);
            snapshotOpts.maxNodes = getInt (params, "maxNodes", 80);

            pruneRefs();
            ++generation;

            juce::Array<juce::var> ancestors;
            juce::Array<juce::Component*> ancestorComponents;

            for (auto* parent = resolution.component->getParentComponent(); parent != nullptr; parent = parent->getParentComponent())
                ancestorComponents.add (parent);

            for (int i = ancestorComponents.size(); --i >= 0;)
            {
                auto ancestor = serializeComponent (*ancestorComponents[i], 0, 0);
                auto compactAncestor = snapshotOpts.mode == "full" ? ancestor : interestingSnapshotTree (ancestor, snapshotOpts);

                if (!compactAncestor.isVoid())
                    ancestors.add (compactAncestor);
            }

            const auto depth = juce::jlimit (0, 16, getInt (params, "depth", 2));
            auto detailTree = serializeComponent (*resolution.component, 0, depth);
            auto detail = snapshotOpts.mode == "full" ? detailTree : interestingSnapshotTree (detailTree, snapshotOpts);

            if (detail.isVoid())
                detail = detailTree;

            auto* detailObject = detail.getDynamicObject();

            if (detailObject == nullptr)
                return error ("describe_failed", "Could not describe target component.");

            juce::String text;
            appendTextSnapshot (text, detail, 0);

            auto children = detailObject->getProperty ("children");
            auto result = object ({ { "generation", generation },
                                    { "mode", snapshotOpts.mode },
                                    { "stateHash", calculateStateHash (detail) },
                                    { "match", summarizeNode (*detailObject) },
                                    { "detail", detail },
                                    { "ancestors", ancestors },
                                    { "children", children },
                                    { "actions", actionHintsForNode (*detailObject) },
                                    { "text", text } });

            return result;
        }
