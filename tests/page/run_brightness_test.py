import asyncio, os, json
from playwright.async_api import async_playwright
EXE = '/opt/pw-browsers/chromium' if os.path.exists('/opt/pw-browsers/chromium') else None
URL = 'http://127.0.0.1:8765/'
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch(executable_path=EXE)
        page = await (await b.new_context(viewport={'width': 390, 'height': 844}, device_scale_factor=2)).new_page()
        errs = []
        page.on('pageerror', lambda e: errs.append(str(e)))
        async def tv(): return json.loads(await (await page.request.get(URL + '_light')).text())
        async def setr(sel, val):
            await page.evaluate(f"()=>{{const s=document.querySelector('{sel}');s.value={val};s.dispatchEvent(new Event('change'))}}")
            await page.wait_for_timeout(300)
        res = {}
        await page.goto(URL); await page.wait_for_timeout(600)
        res['brightness card has only the slider'] = await page.locator('#lv').count() == 1 and await page.locator('#au').count() == 0
        res['slider starts at the TV\'s brightness'] = (await page.input_value('#lv')) == '255'
        await setr('#lv', 120); res['brightness slider reaches the TV'] = (await tv())['level'] == 120
        await page.reload(); await page.wait_for_timeout(600)
        res['brightness comes back after reloading the page'] = (await page.input_value('#lv')) == '120'
        for k, v in res.items(): print('PASS' if v else 'FAIL', k)
        print('page errors:', errs or 'none')
        await b.close()
asyncio.run(main())
