local build_path = vim.fs.joinpath(vim.uv.cwd(), "build/debug");

vim.api.nvim_create_autocmd("FileType", {
    pattern = { "c", "cpp" },
    callback = function()
        vim.opt_local.makeprg = "cmake --build " .. build_path .. " --parallel"
    end,
})
